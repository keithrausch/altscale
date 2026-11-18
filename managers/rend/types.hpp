#ifndef MANAGERS_REND_TYPES_HPP
#define MANAGERS_REND_TYPES_HPP

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <unordered_set>
#include "common/utils/json.hpp"

namespace managers::rend {


struct Interface
{
    std::string name{};
    std::string description{};
    std::string gateway{};
    std::vector<std::string> ip_and_subnets{};

    bool operator==(const Interface&) const = default;

    static Interface from_json(const common::utils::JSON& j)
    {
        Interface ret{};
        ret.name = j.at("name").get<std::string>();
        ret.description = j.value("description", std::string{});
        ret.gateway = j.value("gateway", std::string{});
        ret.ip_and_subnets = j.at("ip_and_subnets").get<std::vector<std::string>>();
        return ret;
    }


    common::utils::JSON to_json() const
    {
        common::utils::JSON j;
        j["name"] = name;
        j["description"] = description;
        j["gateway"] = gateway;
        j["ip_and_subnets"] = ip_and_subnets;
        return j;
    }
};

struct Interfaces
{
    std::unordered_map<std::string /*iface name*/, Interface> interfaces{};

    bool operator==(const Interfaces&) const = default;


    static Interfaces from_json(const common::utils::JSON& j)
    {
        Interfaces ret{};

        if (j.contains("interfaces") && j.at("interfaces").is_array())
        {
            for (const auto& iface_json : j.at("interfaces"))
            {
                auto interface = Interface::from_json(iface_json);
                ret.interfaces.emplace(interface.name, std::move(interface));
            }
        }

        return ret;
    }


    common::utils::JSON to_json() const
    {
        common::utils::JSON j;

        j["interfaces"] = common::utils::JSON::array();

        for (const auto& [iface_name, interface] : interfaces)
        {
            j["interfaces"].push_back(interface.to_json());
        }

        return j;
    }
};


struct UniqueClient
{
    std::string uuid{};
    std::string public_key{};
    Interfaces interfaces{};
    std::string desired_inner_cidr_ip{};


    bool operator==(const UniqueClient&) const = default;


    static UniqueClient from_json(const common::utils::JSON& j)
    {
        UniqueClient ret{};
        ret.uuid = j.at("uuid").get<std::string>();
        ret.public_key = j.at("public_key").get<std::string>();
        ret.interfaces = Interfaces::from_json(j.at("interfaces"));
        ret.desired_inner_cidr_ip = j.at("desired_inner_cidr_ip").get<std::string>();
        return ret;
    }


    common::utils::JSON to_json() const
    {
        common::utils::JSON j;
        j["uuid"] = uuid;
        j["public_key"] = public_key;
        j["interfaces"] = interfaces.to_json();
        j["desired_inner_cidr_ip"] = desired_inner_cidr_ip;
        return j;
    }
};

struct TimestampedClient
{
    std::chrono::system_clock::time_point update_time{};
    UniqueClient unique_client{};
    std::vector<std::string> public_endpoints{};


    bool operator==(const TimestampedClient&) const = default;

    bool maintain_endpoints(const std::string& endpoint, size_t max_history_size = 5)
    {
        auto it = std::find(public_endpoints.begin(), public_endpoints.end(), endpoint);
        bool client_already_known = (it != public_endpoints.end());

        if (client_already_known) // move to front
        {
            std::rotate(public_endpoints.begin(), it, it + 1);
        }
        else // not already known, add
        {
            public_endpoints.insert(public_endpoints.begin(), endpoint);
            if (public_endpoints.size() > max_history_size)
            {
                public_endpoints.pop_back();
            }
        }

        return client_already_known;
    }


    static TimestampedClient from_json(const common::utils::JSON& j)
    {
        TimestampedClient ret{};

        const auto milliseconds = j.at("update_time").get<std::int64_t>();
        std::chrono::system_clock::time_point{std::chrono::milliseconds{milliseconds}};
        ret.unique_client = UniqueClient::from_json(j.at("unique_client"));
        ret.public_endpoints = j.value("public_endpoints", std::vector<std::string>{});
        return ret;
    }


    common::utils::JSON to_json() const
    {
        common::utils::JSON j;

        j["update_time"] =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                update_time.time_since_epoch()
            ).count();
        j["unique_client"] = unique_client.to_json();
        j["public_endpoints"] = public_endpoints;
        return j;
    }
};

// Shared application state protected for multi-threaded access
struct ServerState
{
    std::string rend_vpnI_public_key{};
    std::string rend_vpnI_cidr_ip{};
    std::unordered_map<std::string, TimestampedClient> clients{};


    static ServerState from_json(const common::utils::JSON& j)
    {
        ServerState ret{};

        ret.rend_vpnI_public_key = j.at("rend_vpnI_public_key").get<std::string>();
        ret.rend_vpnI_cidr_ip = j.at("rend_vpnI_cidr_ip").get<std::string>();

        if (j.contains("clients") && j.at("clients").is_array())
        {
            for (const auto& client_json : j.at("clients"))
            {
                auto client = TimestampedClient::from_json(client_json);
                ret.clients.emplace(client.unique_client.uuid, std::move(client));
            }
        }

        return ret;
    }

    common::utils::JSON to_json() const
    {
        common::utils::JSON j;

        // interface information
        j["rend_vpnI_public_key"] = rend_vpnI_public_key;
        j["rend_vpnI_cidr_ip"] = rend_vpnI_cidr_ip;
        j["clients"] = common::utils::JSON::array();

        for (const auto & [uuid, client] : clients)
        {
            j["clients"].push_back(client.to_json());
        }

        return j;
    }
};

}

#endif