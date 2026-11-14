#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>

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

    bool setup(std::unique_ptr<httplib::Server> server_ptr, 
               ServerState &state, 
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

        server->Get("/api/query", [&](const httplib::Request &, httplib::Response &res) {
            std::lock_guard lock(state.mutex);

            res.status = 200;
            nlohmann::json json_response = {{"code", 200},
                                            {"status", "success"}, 
                                            {"server_state", state.to_json()}};
            res.set_content(json_response.dump(), "application/json");
        });

        server->Post("/api/register", [&](const httplib::Request &req, httplib::Response &res) {
            
            ClientState new_client_state;
            try
            {
                nlohmann::json j = nlohmann::json::parse(req.body);
                new_client_state = ClientState::parse(j);
            }
            catch (const nlohmann::json::exception& e)
            {
                std::cerr << "json error: " << e.what() << '\n';
            }

            // check validity
            auto new_uuid = new_client_state.uuid;
            bool valid = (0 == new_uuid.size());
            if (!valid)
            {
                res.status = 400;
                nlohmann::json json_response = {{"code", 400},{"status", "error"}};
                res.set_content(json_response.dump(), "application/json");
                return;
            }

            bool change_detected = false;
            auto now = std::chrono::system_clock::now();

            
            std::lock_guard lock(state.mutex);

            // check existance
            bool already_exists = state.clients.contains(new_uuid);
            if (!already_exists)
            {
                state.clients[new_uuid] = Client{.state = new_client_state, .update_time=now};
                change_detected |= true;
            }
            else // already exists
            {
                auto & old_client = state.clients.at(new_uuid);
                change_detected |= (old_client.state != new_client_state);

                // update existing record
                old_client.update_time = now;
                if (change_detected)
                {
                    old_client.state = new_client_state;
                }
            }

            // indicate success
            res.status = 200;
            nlohmann::json json_response = {{"code", 200},
                                            {"status", "success"}, 
                                            {"already_exists", (already_exists ? "true" : "false")}, 
                                            {"change_detected", (change_detected ? "true" : "false")}, 
                                            {"server_state", state.to_json()}};
            res.set_content(json_response.dump(), "application/json");
        });


        std::cout << "binding " << description << " listener to " << host << ":" << port << "\n";
        had_error |= !server->bind_to_port(host, port);
        
        if (had_error)
        {
            std::cerr << "Failed to bind " << description << " listener to " << host << ":" << port << "\n";
        }

        std::cout << "serving " << root_path << "\n";
        had_error |= (!server->set_mount_point("/", root_path));
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

#endif