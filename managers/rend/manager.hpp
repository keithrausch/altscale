#ifndef MANAGERS_REND_HPP
#define MANAGERS_REND_HPP

#include "common/managers/manager_base.hpp"
#include <chrono>
#include <vector>
#include "managers/rend/server.hpp"
#include "managers/rend/state.hpp"
#include "common/utils/sleeper.hpp"

namespace managers::rend {

struct Params
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
};


class Manager : public common::managers::ManagerBase
{
    public:

    Params params{};
    std::vector<ThreadedServer> servers{};
    ServerState state{};

    Manager(const std::string& name, const std::string& type) : ManagerBase(name, type) {}

    bool init_servers()
    {
        bool had_error = false;

        servers.clear();


        if (params.use_http)
        {
            ThreadedServer server; 
            had_error |= server.setup(std::make_unique<httplib::Server>(), 
                                    state, 
                                    "http", 
                                    params.host_http, 
                                    params.port_http,
                                    params.root_path);
            if (!had_error)
            {
                servers.push_back(std::move(server));
            }
        }
        
        if (params.use_https)
        {
            ThreadedServer server;
            had_error |= server.setup(std::make_unique<httplib::SSLServer>(params.cert_path.string().c_str(), params.key_path.string().c_str()), 
                                    state, 
                                    "https", 
                                    params.host_https, 
                                    params.port_https,
                                    params.root_path);
            if (!had_error)
            {
                servers.push_back(std::move(server));
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

    void read_xml(common::utils::XML& xml) override;
    void process() override;



};

}
#endif