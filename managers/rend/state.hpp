#ifndef COMMON_STATE_HPP
#define COMMON_STATE_HPP

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

#include <nlohmann/json.hpp>

// namespace common {

struct ClientState
{
    struct InterfaceInfo
    {
        std::string description{};
        std::string gateway{};
        std::vector<std::string> ip_and_subnets{};

        bool operator==(const InterfaceInfo&) const = default;
    };
    
    std::string uuid{};
    std::unordered_map<std::string /*iface name*/, InterfaceInfo> iface_infos{};
    std::vector<std::string> public_endpoints{};

    bool operator==(const ClientState&) const = default;


    static ClientState from_json(const nlohmann::json& j)
    {
        ClientState state{};

        // uuid
        state.uuid = j.at("uuid").get<std::string>();

        // public endpoints
        state.public_endpoints = j.value("public_endpoints", std::vector<std::string>{});

        // interface information
        if (j.contains("iface_infos") && j.at("iface_infos").is_object())
        {
            for (const auto& [iface_name, iface_json] : j.at("iface_infos").items())
            {
                ClientState::InterfaceInfo info{};

                info.description = iface_json.value("description", "");
                info.gateway = iface_json.value("gateway", "");
                info.ip_and_subnets = iface_json.value("ip_and_subnets", std::vector<std::string>{});

                state.iface_infos.emplace(iface_name, std::move(info));
            }
        }

        return state;
    }


    nlohmann::json to_json() const
    {
        nlohmann::json j;

        // uuid
        j["uuid"] = uuid;

        // public endpoints
        j["public_endpoints"] = public_endpoints;

        // interface information
        j["iface_infos"] = nlohmann::json::object();

        for (const auto& [iface_name, info] : iface_infos)
        {
            j["iface_infos"][iface_name] = {
                {"description", info.description},
                {"gateway", info.gateway},
                {"ip_and_subnets", info.ip_and_subnets}
            };
        }

        return j;
    }
};

struct Client
{
    ClientState state{};
    std::chrono::system_clock::time_point update_time{};
};

// Shared application state protected for multi-threaded access
struct ServerState {
    std::mutex mutex{};
    std::unordered_map<std::string, Client> clients{};

    nlohmann::json to_json() const
    {
        // std::lock_guard lock(mutex);
        nlohmann::json j;

        // interface information
        j["clients"] = nlohmann::json::object();

        for (const auto & [uuid, client] : clients)
        {
            j["clients"][uuid] = client.state.to_json();
        }

        return j;
    }
};

// }

#endif