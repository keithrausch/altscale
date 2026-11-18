#ifndef MANAGERS_REND_SERVER_HPP
#define MANAGERS_REND_SERVER_HPP

#include <iostream>
#include <memory>
#include <thread>
#include <filesystem>

#include "httplib.h"
#include "common/utils/signal.hpp"
#include "common/utils/json.hpp"

namespace common::utils {

class ThreadedServer
{
    std::unique_ptr<httplib::Server> server{};
    std::jthread thread{};

    public:
    ThreadedServer() = default;
    ThreadedServer(const ThreadedServer& rhs) = delete;
    ThreadedServer(ThreadedServer&& rhs) = default;
    ThreadedServer& operator=(const ThreadedServer& rhs) = delete;
    ThreadedServer& operator=(ThreadedServer&& rhs) = default;

    bool bind(std::unique_ptr<httplib::Server> server_ptr, 
               const std::string& description, 
               const std::string& host, 
               uint16_t port,
               const std::filesystem::path& root_path)
    {
        bool had_error = false;
        had_error |= thread.joinable(); // already running
        had_error |= (!server_ptr);

        if (had_error)
        {
            return had_error;
        }

        server = std::move(server_ptr);


        had_error |= (!server->bind_to_port(host, port));

        had_error |= (!server->is_valid());
        
        if (!had_error)
        {
            std::cout << "binding " << description << " listener to " << host << ":" << port << " (success)\n";
        }
        else
        {
            std::cerr << "binding " << description << " listener to " << host << ":" << port << " (FAILED)\n";
        }

        if (!root_path.empty())
        {
            // std::cout << "serving " << root_path << '\n';
            had_error |= (!server->set_mount_point("/", root_path));
        }
        if (had_error)
        {
            std::cerr << "error mounting root folder\n";
            return had_error;
        }

        return had_error;
    };

    bool run()
    {
        bool had_error = false;
        had_error |= (!server);
        had_error |= thread.joinable();

        if (!had_error)
        {
            thread = std::jthread([this]() {server->listen_after_bind();});
        }

        return had_error;
    }

    void stop()
    {
        if (server)
        {
            server->stop();
        }

        if (thread.joinable())
        {
            thread.join();
        }
    }

    ~ThreadedServer()
    {
        stop();
    }
};

}

#endif