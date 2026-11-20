#ifndef MANAGERS_CONTROLLER_MANAGER_HPP
#define MANAGERS_CONTROLLER_MANAGER_HPP

#include "common/managers/manager_base.hpp"
#include <chrono>
#include <vector>
#include "common/utils/sleeper.hpp"
#include "managers/rend/types.hpp"
#include "common/utils/wireguard.hpp"

namespace managers::controller {

struct StunParams
{
    uint16_t port{};
    double scan_duration_s{5.0};
};

struct WireGuardRemoteParams
{
    uint16_t wireguard_port{};

};
struct WireGuardLocalParams
{
    std::string vpnI_iface_name{"wg-rend"};
    uint16_t vpnI_listening_port{};
    std::string vpnI_ip{};
    uint8_t vpnI_subnet{};
    // std::string desired_outer_ip{};
    // uint16_t desired_outer_port{};

};

struct WireGuardParams
{
    WireGuardRemoteParams remote{};
    WireGuardLocalParams local{};
};

struct Params
{
    std::filesystem::path uuid_path{};
    std::string rend_host{};
    StunParams stun{};
    WireGuardParams wg{};
};


class Manager : public common::managers::ManagerBase
{
    public:

    Params params{};
    std::string uuid{};
    common::utils::WireGuard rend_if{};

    
    Manager(const std::string& name, const std::string& type) : ManagerBase(name, type) {}    
    void read_xml(common::utils::XML& xml) override;
    void process() override;
    
    std::pair<std::optional<std::string>, std::optional<managers::rend::ServerState>> send_registration(const std::string & json_body);

    std::string generate_random_string(size_t length);

};

}
#endif