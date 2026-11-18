#include "managers/rend/manager.hpp"

#include "common/utils/signal.hpp"
#include <algorithm>
#include "common/utils/wireguard.hpp"

namespace managers::rend {

    void Manager::read_xml(common::utils::XML& xml)
    {
        auto r = xml.root();

        r.require(params.server.use_http, "params.server.use_http");
        r.require(params.server.host_http, "params.server.host_http");
        r.require(params.server.port_http, "params.server.port_http");

        r.require(params.server.use_https, "params.server.use_https");
        r.require(params.server.host_https, "params.server.host_https");
        r.require(params.server.port_https, "params.server.port_https");
        r.require(params.server.cert_path, "params.server.cert_path");
        r.require(params.server.key_path, "params.server.key_path");

        r.require(params.server.root_path, "params.server.root_path");

        r.require(params.server.broadcast_duration_s, "params.server.broadcast_duration_s");

        r.require(params.wg.rend_vpnI_iface_name, "params.wireguard.rend_vpnI_iface_name");
        r.require(params.wg.rend_vpnI_cidr_ip, "params.wireguard.rend_vpnI_cidr_ip");
        r.require(params.wg.rend_vpnI_listening_port, "params.wireguard.rend_vpnI_listening_port");
    }

    void Manager::process()
    {
        // init

        //
        // servers
        //

        if (init_servers())
        {
            std::cerr << "error setting up servers\n";

            // this is basically a fatal problem anyways
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }

        //
        // wireguard
        //
        if (Error::SUCCESS != common::utils::WireGuard::KeyPair::generate(keys))
        {
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }

        std::cout << "Private key: "
                << keys.private_key_base64 << '\n';

        std::cout << "Public key: "
                << keys.public_key_base64 << '\n';

        rend_if = std::move(common::utils::WireGuard(params.wg.rend_vpnI_iface_name));

        if (Error::SUCCESS != rend_if.create())
        {
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }

        if (Error::SUCCESS != rend_if.set_private_key(keys.private_key_base64))
        {
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }

        if (Error::SUCCESS != rend_if.set_listen_port(params.wg.rend_vpnI_listening_port))
        {
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }

        if (Error::SUCCESS != rend_if.set_ip(params.wg.rend_vpnI_cidr_ip))
        {
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }

        if (Error::SUCCESS != rend_if.set_up_down(true))
        {
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }


        common::utils::Sleeper sleeper{};
        auto cb_sleeper = create_stop_callback([&sleeper](){ sleeper.wake();});

        // loop
        while (running)
        {
            sleeper.sleep_for(params.server.broadcast_duration_s);
        }
    }

    void Manager::configure_server_endpoints(const std::unique_ptr<httplib::Server>& server)
    {
        if (!server)
        {
            return;
        }

        server->Post("/api/stop", [&](const httplib::Request &, httplib::Response &res) {
            std::lock_guard lock(state_mutex);

            res.status = 200;
            common::utils::JSON json_response = {{"code", 200},
                                            {"status", "success"}};
            res.set_content(json_response.dump(), "application/json");

            common::utils::GlobalSignalHandler::instance().request_stop();
        });

        server->Post("/api/query", [&](const httplib::Request &, httplib::Response &res) {
            std::lock_guard lock(state_mutex);

            res.status = 200;
            common::utils::JSON json_response = {{"code", 200},
                                            {"status", "success"},
                                            {"server_state", state.to_json()}};
            res.set_content(json_response.dump(), "application/json");
        });

        server->Post("/api/register", [&](const httplib::Request &req, httplib::Response &res) {

            auto client_endpoint = req.remote_addr + ":" + std::to_string(req.remote_port);

            UniqueClient new_client;
            try
            {
                common::utils::JSON j = common::utils::JSON::parse(req.body);
                new_client = UniqueClient::from_json(j);
            }
            catch (const common::utils::JSON::exception& e)
            {
                std::cerr << "json error: " << e.what() << '\n';
            }

            // check validity
            auto new_uuid = new_client.uuid;
            bool valid = (0 != new_uuid.size());
            if (!valid)
            {
                res.status = 400;
                common::utils::JSON json_response = {{"code", 400},{"status", "error"},{"reason", "uuid empty"}};
                res.set_content(json_response.dump(), "application/json");
                return;
            }

            // check endpoint not already in use by someone else
            bool desired_endpoint_occupied = false;
            for (const auto & [uuid, client] : state.clients)
            {
                desired_endpoint_occupied |= ((uuid != new_uuid) && (client.unique_client.desired_inner_cidr_ip == new_client.desired_inner_cidr_ip));

            }
            if (desired_endpoint_occupied)
            {
                res.status = 400;
                common::utils::JSON json_response = {{"code", 400},{"status", "error"},{"reason", "desired ip already occupied"}};
                res.set_content(json_response.dump(), "application/json");
                return;
            }

            // TODO check if the subnet that the client wants to be on is compatible with our subnet

            bool client_changed = false;
            bool client_public_endpoint_changed = false;
            auto now = std::chrono::system_clock::now();


            std::lock_guard lock(state_mutex);

            // check existance
            bool already_exists = state.clients.contains(new_uuid);
            if (already_exists)
            {
                auto & old_client = state.clients.at(new_uuid);

                // must delete old client first before we start updating keys
                if (old_client.unique_client.public_key != new_client.public_key)
                {
                    std::cout << "public key changed. removing peer with old key\n";
                    if (Error::SUCCESS != rend_if.remove_peer(old_client.unique_client.public_key))
                    {
                        std::cerr << "error removing peer with public key " << std::quoted(old_client.unique_client.public_key) << '\n';

                        res.status = 400;
                        common::utils::JSON json_response = {{"code", 400},{"status", "error"},{"reason", "could not unregister old public key"}};
                        res.set_content(json_response.dump(), "application/json");
                        return;

                    }
                }

                client_changed |= (old_client.unique_client != new_client);

                bool client_already_known = old_client.maintain_endpoints(client_endpoint);
                // change_detected |= !client_already_known;
                // client_public_endpoint_changed |= client_already_known // ???????????
            }

            if (client_changed)
            {
                std::cout << "CHANGE DETECTED. NEW CLIENT: " << new_client.to_json().dump() << '\n';
                if (Error::SUCCESS != rend_if.add_peer(new_client.public_key, {new_client.desired_inner_cidr_ip}
                                                                         /* ip and port left unknown for client ot connect first */ ))
                {
                    // TODO wipe this client from the record?
                    state.clients.erase(new_uuid); // SEE ME


                    std::cerr << "error adding peer. uuid: "<<std::quoted(new_client.uuid)<<'\n';

                    res.status = 400;
                    common::utils::JSON json_response = {{"code", 400},{"status", "error"},{"reason", "internal server error ;)"}};
                    res.set_content(json_response.dump(), "application/json");
                    return;
                }
            }

            // no errors. finally add this new client to our database
            if (!already_exists)
            {
                state.clients[new_uuid] = TimestampedClient{.update_time=now, .unique_client = new_client};
                client_changed |= true;
            }
            else
            {
                // update existing record with new interfaces
                auto &old_client = state.clients.at(new_uuid);
                old_client.update_time = now;
                if (client_changed)
                {
                    old_client.unique_client = new_client;
                }
            }

            state.rend_vpnI_public_key = keys.public_key_base64;
            state.rend_vpnI_cidr_ip = params.wg.rend_vpnI_cidr_ip;

            // indicate success
            res.status = 200;
            common::utils::JSON json_response = {{"code", 200},
                                            {"status", "success"},
                                            {"already_exists", (already_exists ? "true" : "false")},
                                            {"client_changed", (client_changed ? "true" : "false")},
                                            {"server_state", state.to_json()}};
            res.set_content(json_response.dump(), "application/json");
        });

    }


    bool Manager::init_servers()
    {
        bool had_error = false;

        servers.clear();


        if (params.server.use_http)
        {
            common::utils::ThreadedServer threaded_server;
            auto inner_server = std::make_unique<httplib::Server>();
            configure_server_endpoints(inner_server);
            had_error |= threaded_server.bind(std::move(inner_server),
                                    "http",
                                    params.server.host_http,
                                    params.server.port_http,
                                    params.server.root_path);
            if (!had_error)
            {
                servers.push_back(std::move(threaded_server));
            }
        }

        if (params.server.use_https)
        {
            common::utils::ThreadedServer threaded_server;
            std::unique_ptr<httplib::Server> inner_server = std::make_unique<httplib::SSLServer>(params.server.cert_path.string().c_str(),
                                                                     params.server.key_path.string().c_str());
            configure_server_endpoints(inner_server);
            had_error |= threaded_server.bind(std::move(inner_server),
                                    "https",
                                    params.server.host_https,
                                    params.server.port_https,
                                    params.server.root_path);
            if (!had_error)
            {
                servers.push_back(std::move(threaded_server));
            }
        }

        if (had_error)
        {
            return had_error;
        }

        for (auto & server : servers)
        {
            server.run();
        }

        return had_error;
    }
}
static common::managers::ManagerRegisterer<managers::rend::Manager> reg("rend");