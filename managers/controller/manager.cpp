#include "managers/controller/manager.hpp"


#include "httplib.h"
#include "common/utils/signal.hpp"
#include "managers/controller/interfaces.hpp"
#include "managers/rend/types.hpp"
#include "common/utils/file_io.hpp"
#include <random>
#include "common/utils/wireguard.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <iostream>
#include <string>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;

namespace managers::controller {

    void Manager::read_xml(common::utils::XML& xml)
    {
        auto r = xml.root();

        r.require(params.uuid_path, "params.uuid_path");
        r.require(params.rend_host, "params.rend_host");

        r.require(params.stun.port, "params.stun.port");
        r.require(params.stun.scan_duration_s, "params.stun.scan_duration_s");

        r.require(params.wg.remote.wireguard_port, "params.wireguard.remote.wireguard_port");
        r.require(params.wg.local.vpnI_iface_name, "params.wireguard.local.vpnI_iface_name");
        r.require(params.wg.local.vpnI_listening_port, "params.wireguard.local.vpnI_listening_port");
        r.require(params.wg.local.vpnI_ip, "params.wireguard.local.vpnI_ip");
        r.require(params.wg.local.vpnI_subnet, "params.wireguard.local.vpnI_subnet");
        // r.require(params.wg.desired_outer_ip, "params.wireguard.desired_outer_ip");
        // r.require(params.wg.desired_outer_port, "params.wireguard.desired_outer_port");


        // r.require(params.wireguard.local_iface_name, "params.wireguard.local_iface_name");
        // r.require(params.wireguard.local_ip, "params.wireguard.local_ip");
        // r.require(params.wireguard.local_ip_prefix_length, "params.wireguard.local_ip_prefix_length");
    }

    std::pair<std::optional<std::string>, std::optional<managers::rend::ServerState>> Manager::send_registration(const std::string& json_body)
    {
        std::optional<managers::rend::ServerState> server_state = std::nullopt;

        std::optional<std::string> resolved_ip = std::nullopt;

        try {
            net::io_context ioc;

            tcp::resolver resolver(ioc);
            beast::tcp_stream stream(ioc);

            // 1. Resolve domain OR parse raw IP string
            const std::string host = params.rend_host;
            const uint16_t port = params.stun.port;
            auto const results = resolver.resolve(host, std::to_string(port));

            // TODO WE NEED TO RESOLVE THE WG PORT ALSO BUT WE ARE JUST USING THE STUN PORT FOR IT FOR NOW

            // 2. Connect to the resolved endpoints and get the connected endpoint
            tcp::endpoint connected_endpoint = stream.connect(results);

            // 3. Record the server IP address
            resolved_ip = connected_endpoint.address().to_string();
            // std::cout << "Connected to server IP: " << *resolved_ip << std::endl;

            // 4. Build the HTTP POST request
            http::request<http::string_body> req{http::verb::post, "/api/register", 11};
            req.set(http::field::host, host);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req.set(http::field::content_type, "application/json");
            req.body() = json_body;
            req.prepare_payload(); // Automatically sets Content-Length

            // 5. Send the HTTP request
            http::write(stream, req);

            // 6. Receive the HTTP response
            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);

//             std::cout << "Status: " << res.result_int() << std::endl;

            if (200 == res.result_int())
            {
                auto total_json = common::utils::JSON::parse(res.body());
                auto server_state_json = total_json.at("server_state");
                server_state = managers::rend::ServerState::from_json(server_state_json);
            }
            else
            {
                std::cerr << "server response: " << res.body() << std::endl;
            }

            // 7. Gracefully close the socket
            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        }
        catch (const std::exception& e)
        {
            std::cerr << "HTTP error: " << e.what() << std::endl;
        }

        return std::make_pair(resolved_ip, server_state);
    }

    void Manager::process()
    {
        // init

        //
        // uuid
        //

        if (params.uuid_path.empty())
        {
            std::cerr << "uuid path empty\n";
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }

        auto maybe_uuid = common::utils::read_file(params.uuid_path);
        if (maybe_uuid)
        {
            uuid = *maybe_uuid;
        }
        else
        {
            std::cout << "error reading uuid file\n";
            std::cout << "generating new uuid and writing to " << params.uuid_path << '\n';
            uuid = generate_random_string(32);
            bool had_error = common::utils::write_file(params.uuid_path, uuid);
            if (had_error)
            {
                std::cerr << "could not write uuid to " << params.uuid_path << '\n';
                common::utils::GlobalSignalHandler::instance().request_stop();
                return;
            }
        }

        if (uuid.empty())
        {
            std::cerr << "uuid empty\n";
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }

        //
        // wireguard
        //

        common::utils::WireGuard::KeyPair keys;
        if (Error::SUCCESS != common::utils::WireGuard::KeyPair::generate(keys))
        {
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }

        std::cout << "Private key: "
                << keys.private_key_base64 << '\n';

        std::cout << "Public key: "
                << keys.public_key_base64 << '\n';


        rend_if = std::move(common::utils::WireGuard(params.wg.local.vpnI_iface_name));

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

        if (Error::SUCCESS != rend_if.set_listen_port(params.wg.local.vpnI_listening_port))
        {
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }

        if (Error::SUCCESS != rend_if.set_up_down(false))
        {
            common::utils::GlobalSignalHandler::instance().request_stop();
            return;
        }

        common::utils::Sleeper sleeper{};
        auto scb_sleeper = create_stop_callback([&sleeper](){sleeper.wake();});

        auto sleep_duration = std::chrono::duration<double>(params.stun.scan_duration_s);

        std::string last_server_ip{};

        auto run_impl = [&]()
        {
            auto ifaces = get_network_interfaces();
            managers::rend::UniqueClient client{.uuid=uuid,
                .public_key = keys.public_key_base64,
                .interfaces = ifaces,
                .desired_inner_ip = params.wg.local.vpnI_ip};

            auto [resolved_ip, server_state] = send_registration(client.to_json().dump());

            if (!resolved_ip)
            {
                std::cout << "could not resolve ip\n";
            }

            if (!server_state)
            {
                std::cout << "server state bad\n";
            }



            if (!resolved_ip || !server_state)
            {
                return;
            }

            bool change_detected = false;
            if (! resolved_ip->empty())
            {
                change_detected |= (last_server_ip != *resolved_ip);
            }
            
            if (change_detected)
            {
                std::cout << "server ip changed from " << std::quoted(last_server_ip) << " to "<< std::quoted(*resolved_ip) << '\n';
                if (Error::SUCCESS != rend_if.set_ip(params.wg.local.vpnI_ip, params.wg.local.vpnI_subnet))
                {
                    return;
                }

                // change detected
                auto peer_cidr_ip = common::utils::net::make_cidr_full_length(server_state->rend_vpnI_ip);
                if (Error::SUCCESS != rend_if.add_peer(server_state->rend_vpnI_public_key,
                                                   {peer_cidr_ip},
                                                   *resolved_ip,
                                                    params.wg.remote.wireguard_port
                                                   ))
                {
                    return;
                }

                if (Error::SUCCESS != rend_if.set_up_down(true))
                {
                    return;
                }
            }

            // everything must go ok or else well try it all agin
            last_server_ip = *resolved_ip;
        };

        // loop
        while (running)
        {
            run_impl();

            sleeper.sleep_for(sleep_duration);
        }
    }

    // written by AI
    std::string Manager::generate_random_string(size_t length)
    {
        const std::string charset =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

        // Seed the engine with a hardware entropy source
        std::random_device rd;
        std::mt19937 generator(rd());

        // Create a uniform distribution across the character set
        std::uniform_int_distribution<size_t> distribution(0, charset.size() - 1);

        std::string result;
        result.reserve(length);

        for (size_t i = 0; i < length; ++i) {
            result += charset[distribution(generator)];
        }

        return result;

    }

}
static common::managers::ManagerRegisterer<managers::controller::Manager> reg("controller");