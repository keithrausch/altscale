// WRITTEN BY AI
#ifndef MANAGERS_CONTROLLER_INTERFACES_HPP
#define MANAGERS_CONTROLLER_INTERFACES_HPP

#include "managers/rend/types.hpp"

#ifdef __linux__

#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace managers::controller {

managers::rend::Interfaces get_network_interfaces()
{
    managers::rend::Interfaces result{};

    ifaddrs* interfaces = nullptr;

    if (getifaddrs(&interfaces) != 0)
        return result;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    for (auto* iface = interfaces;
         iface != nullptr;
         iface = iface->ifa_next)
    {
        if (!iface->ifa_name)
            continue;

        std::string name = iface->ifa_name;

        // Get or create interface entry.
        auto& info = result.interfaces[name];

        info.name = name;

        // ------------------------------------------------
        // Interface information (future use)
        // ------------------------------------------------

        bool up = iface->ifa_flags & IFF_UP;
        bool loopback = iface->ifa_flags & IFF_LOOPBACK;
        unsigned int index = if_nametoindex(name.c_str());
        unsigned int mtu = 0;
        std::string mac_address;

        if (sock >= 0)
        {
            ifreq request{};

            strncpy(
                request.ifr_name,
                name.c_str(),
                IFNAMSIZ - 1
            );

            if (ioctl(sock, SIOCGIFMTU, &request) == 0)
                mtu = request.ifr_mtu;
        }

        (void)up;
        (void)loopback;
        (void)index;
        (void)mtu;
        (void)mac_address;

        if (!iface->ifa_addr)
            continue;

        int family = iface->ifa_addr->sa_family;

        // ------------------------------------------------
        // IPv4
        // ------------------------------------------------

        if (family == AF_INET)
        {
            auto* addr = reinterpret_cast<sockaddr_in*>(
                iface->ifa_addr
            );

            char address_buffer[INET_ADDRSTRLEN]{};

            inet_ntop(
                AF_INET,
                &addr->sin_addr,
                address_buffer,
                sizeof(address_buffer)
            );

            std::string address = address_buffer;

            if (iface->ifa_netmask)
            {
                auto* mask = reinterpret_cast<sockaddr_in*>(
                    iface->ifa_netmask
                );

                uint32_t mask_value =
                    ntohl(mask->sin_addr.s_addr);

                unsigned int prefix = 0;

                while (mask_value & 0x80000000)
                {
                    ++prefix;
                    mask_value <<= 1;
                }

                address += "/" + std::to_string(prefix);
            }

            info.ip_and_subnets.push_back(address);
        }

        // ------------------------------------------------
        // IPv6
        // ------------------------------------------------

        else if (family == AF_INET6)
        {
            auto* addr = reinterpret_cast<sockaddr_in6*>(
                iface->ifa_addr
            );

            char address_buffer[INET6_ADDRSTRLEN]{};

            inet_ntop(
                AF_INET6,
                &addr->sin6_addr,
                address_buffer,
                sizeof(address_buffer)
            );

            std::string address = address_buffer;

            if (iface->ifa_netmask)
            {
                auto* mask = reinterpret_cast<sockaddr_in6*>(
                    iface->ifa_netmask
                );

                unsigned int prefix = 0;

                for (const auto byte : mask->sin6_addr.s6_addr)
                {
                    unsigned char value = byte;

                    while (value & 0x80)
                    {
                        ++prefix;
                        value <<= 1;
                    }
                }

                address += "/" + std::to_string(prefix);
            }

            info.ip_and_subnets.push_back(address);
        }

        // ------------------------------------------------
        // MAC address (future use)
        // ------------------------------------------------

        else if (family == AF_PACKET)
        {
            auto* packet = reinterpret_cast<sockaddr_ll*>(
                iface->ifa_addr
            );

            std::ostringstream mac;

            for (int i = 0; i < packet->sll_halen; ++i)
            {
                if (i)
                    mac << ':';

                mac << std::hex
                    << std::setfill('0')
                    << std::setw(2)
                    << static_cast<unsigned int>(
                        packet->sll_addr[i]
                    );
            }

            mac_address = mac.str();

            (void)mac_address;
        }
    }

    if (sock >= 0)
        close(sock);

    freeifaddrs(interfaces);

    return result;
}

} // namespace managers::controller

#endif // __linux__

#endif // MANAGERS_CONTROLLER_INTERFACES_HPP
