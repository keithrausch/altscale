#include "httplib.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <filesystem>

#include "sleeper.hpp"
#include "state.hpp"
#include "server.hpp"
#include "signal.hpp"

SignalReceiver global_signal_receiver{}; // it is NOT lock-free. not sure i care.

extern "C" void signal_handler(int signal) {
    global_signal_receiver.last_signal = signal;
    global_signal_receiver.notifier.release();
}

struct Config
{
    bool use_http{true};
    std::string host_http{"0.0.0.0"};
    uint16_t port_http{8080};

    bool use_https{true};
    std::string host_https{"0.0.0.0"};
    uint16_t port_https{8443};
    std::filesystem::path cert_path{"certs/certs/cert.pem"};
    std::filesystem::path key_path{"certs/certs/key.pem"};

    std::filesystem::path root_path{"./static"};

    double broadcast_duration_s{5.0};

    bool load(const std::filesystem::path& config_path)
    {
        bool had_error = false; 
        return had_error;
    }
};


struct Application
{
    std::atomic<bool> running{};
    Config config{};
    std::vector<ThreadedServer> servers{};
    ServerState state{};

    std::vector<std::jthread> user_threads{};
    Sleeper user_notifier{};
};

bool init_servers(Application& app)
{
    bool had_error = false;

    app.servers.clear();

    auto & config = app.config;

    if (config.use_http)
    {
        ThreadedServer server; 
        had_error |= server.setup(std::make_unique<httplib::Server>(), 
                                  app.state, 
                                  "http", 
                                  config.host_http, 
                                  config.port_http,
                                  config.root_path);
        if (!had_error)
        {
            app.servers.push_back(std::move(server));
        }
    }
    
    if (config.use_https)
    {
        ThreadedServer server; // std::make_unique<httplib::SSLServer>(config.cert_path.string(), config.key_path.string())
        had_error |= server.setup(std::make_unique<httplib::Server>(), 
                                  app.state, 
                                  "https", 
                                  config.host_https, 
                                  config.port_https,
                                  config.root_path);
        if (!had_error)
        {
            app.servers.push_back(std::move(server));
        }
    }

    if (had_error)
    {
        return had_error;
    }

    for (auto & server : app.servers)
    {
        server.run();
    }

    return had_error;
}

bool init_broadcast(Application& app)
{
    bool had_error = false;

    auto blocking_broadcast = [&app]()
    {
        auto sleep_duration = std::chrono::duration<double>(app.config.broadcast_duration_s);
        
        while (app.running)
        {
            //  httplib::Client cli("localhost", port);
            //  cli.Post("/command", "BOOST_MODE", "text/plain");
            //  if (auto res = cli.Get("/status")) {
            //      std::cout << "[Client Check] Status: " << res->status 
            //                << " | Body: " << res->body << "\n";
            //  }
            
            std::cout << "rend\n";
            app.user_notifier.sleep_for(sleep_duration);
        }
    };

    app.user_threads.emplace_back(blocking_broadcast);

    return had_error;
}

bool main_impl(Application& app, const std::filesystem::path& config_path)
{
    bool had_error = false;

    Config config{};
    
    // config
    if (!config_path.empty())
    {
        std::cout << "loading config from " << config_path << "\n";
        had_error |= config.load(config_path);
    }
    else
    {
        std::cout << "using default config\n";
    }

    if (had_error)
    {
        std::cerr << "configuration failure\n";
        return had_error;
    }

    app.running = true;

    // servers
    had_error |= init_servers(app);
    if (had_error)
    {
        std::cerr << "setup failure\n";
        return had_error;
    }

    had_error |= init_broadcast(app);
    if (had_error)
    {
        std::cerr << "misc failure\n";
        return had_error;
    }

    global_signal_receiver.notifier.acquire();
    std::cout << "got signal: " << global_signal_receiver.last_signal << "\n";
    app.running = false;
    app.user_notifier.wake();

    return had_error;
}

int main(int argc, char* argv[])
{
    if (std::signal(SIGINT, signal_handler) == SIG_ERR)
    {
        std::cerr << "error installing SIGINT handler\n";
    }

    std::cout << "starting...\n";

    std::cout << "working directory: " << std::filesystem::current_path() << '\n';

    std::filesystem::path config_path = "";
    if (argc >= 2)
    {
        config_path = argv[1];
    }

    Application app;

    bool had_error = main_impl(app, config_path);
    if (had_error)
    {
        std::cerr << "had error\n";
    }

    std::cout << "exiting\n";

    return (had_error ? 1 : 0);
}
