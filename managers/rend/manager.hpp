#ifndef MANAGERS_REND_MANAGER_HPP
#define MANAGERS_REND_MANAGER_HPP

#include "common/managers/manager_base.hpp"
#include <chrono>
#include <vector>
#include "common/utils/server.hpp"
#include "managers/rend/types.hpp"
#include "common/utils/sleeper.hpp"
#include "common/utils/wireguard.hpp"

namespace managers::rend {

struct ServerParams
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

    std::chrono::duration<double> broadcast_duration_s{5.0};
};

struct WireGuardParams
{
    std::string rend_vpnI_iface_name{"wg-rend"};
    std::string rend_vpnI_cidr_ip{"10.10.0.1/24"};
    uint16_t rend_vpnI_listening_port{51820};
};

struct Params
{
    ServerParams server{};
    WireGuardParams wg{};

};


class Manager : public common::managers::ManagerBase
{
    public:

    Params params{};
    std::vector<common::utils::ThreadedServer> servers{};
    ServerState state{};
    std::mutex state_mutex{};
    common::utils::WireGuard::KeyPair keys{};
    common::utils::WireGuard rend_if{};

    Manager(const std::string& name, const std::string& type) : ManagerBase(name, type) {}
    void read_xml(common::utils::XML& xml) override;
    void process() override;
    
    
    void configure_server_endpoints(const std::unique_ptr<httplib::Server>& server);
    bool init_servers();

};

}
#endif