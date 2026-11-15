#include "managers/rend/manager.hpp"

#include "common/utils/signal.hpp"

namespace managers::rend {

    void Manager::read_xml(common::utils::XML& xml)
    {
        auto p = xml.root().child("params");

        p.require(params.use_http, "use_http");
        p.require(params.host_http, "host_http");
        p.require(params.port_http, "port_http");
        p.require(params.use_https, "use_https");
        p.require(params.host_https, "host_https");
        p.require(params.port_https, "port_https");
        p.require(params.cert_path, "cert_path");
        p.require(params.key_path, "key_path");
        p.require(params.root_path, "root_path");
        p.require(params.broadcast_duration_s, "broadcast_duration_s");
    }

    void Manager::process()
    {
        // init
        bool had_error = false;

        had_error |= init_servers();
        if (had_error)
        {
            std::cerr << "error setting up servers\n";
            common::utils::GlobalSignalHandler::instance().request_stop();
        }

        common::utils::Sleeper sleeper{};

        register_stop_callback([&sleeper](){sleeper.wake();});

        auto sleep_duration = std::chrono::duration<double>(params.broadcast_duration_s);

        // loop
        while (running)
        {
            sleeper.sleep_for(sleep_duration);
            // std::cout << "server process\n";
        }
    }

}
static common::managers::ManagerRegisterer<managers::rend::Manager> reg("rend");