#ifndef COMMON_UTILS_WIREGUARD_HELPER_HPP
#define COMMON_UTILS_WIREGUARD_HELPER_HPP

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <net/if.h>
#include <linux/rtnetlink.h>
#include <libmnl/libmnl.h>
#include <memory>
#include "common/utils/error.hpp"
#include <vector>

//
#include <linux/wireguard.h>
#include <linux/genetlink.h>
#include <linux/netlink.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/if.h>

#include <openssl/evp.h>
#include <openssl/crypto.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <string>
#include <cstdint>

namespace common::utils {

namespace net
{
    // accepts:
    // 192.168.1.10:51820
    // example.com:51820 — if you later decide to support hostnames, though the implementation below validates IP addresses only.
    // [2001:db8::1]:51820
    // [fe80::1]:51820

    static Error parse_endpoint(
    const std::string& endpoint,
    std::string& ip,
    uint16_t& port)
    {
        std::string address;
        std::string port_str;

        if (endpoint.empty())
        {
            return Error::ERROR;
        }

        if (endpoint.front() == '[')
        {
            // IPv6 format: [address]:port
            auto close = endpoint.find(']');

            if (close == std::string::npos ||
                close + 1 >= endpoint.size() ||
                endpoint[close + 1] != ':')
            {
                return Error::ERROR;
            }

            address = endpoint.substr(1, close - 1);
            port_str = endpoint.substr(close + 2);

            in6_addr addr6{};

            if (inet_pton(AF_INET6, address.c_str(), &addr6) != 1)
            {
                return Error::ERROR;
            }
        }
        else
        {
            // IPv4 format: address:port
            auto colon = endpoint.find(':');

            if (colon == std::string::npos ||
                endpoint.find(':', colon + 1) != std::string::npos)
            {
                return Error::ERROR;
            }

            address = endpoint.substr(0, colon);
            port_str = endpoint.substr(colon + 1);

            in_addr addr4{};

            if (inet_pton(AF_INET, address.c_str(), &addr4) != 1)
            {
                return Error::ERROR;
            }
        }

        if (port_str.empty())
        {
            return Error::ERROR;
        }

        unsigned int parsed_port = 0;

        auto [ptr, ec] = std::from_chars(
            port_str.data(),
            port_str.data() + port_str.size(),
            parsed_port
        );

        if (ec != std::errc{} ||
            ptr != port_str.data() + port_str.size() ||
            parsed_port == 0 ||
            parsed_port > UINT16_MAX)
        {
            return Error::ERROR;
        }

        ip = std::move(address);
        port = static_cast<uint16_t>(parsed_port);

        return Error::SUCCESS;
    }



    // accepts
    // 192.168.1.0/24
    // 10.0.0.1/32
    // 2001:db8::/64
    // fd00::1/128
static Error parse_cidr_ip(
    const std::string& allowed_ip,
    std::string& ip,
    uint8_t& prefix_length)
{
    auto slash = allowed_ip.find('/');

    if (slash == std::string::npos ||
        slash == 0 ||
        slash == allowed_ip.size() - 1 ||
        allowed_ip.find('/', slash + 1) != std::string::npos)
    {
        return Error::ERROR;
    }

    std::string address = allowed_ip.substr(0, slash);
    std::string prefix_str = allowed_ip.substr(slash + 1);

    unsigned int parsed_prefix = 0;

    auto [ptr, ec] = std::from_chars(
        prefix_str.data(),
        prefix_str.data() + prefix_str.size(),
        parsed_prefix
    );

    if (ec != std::errc{} ||
        ptr != prefix_str.data() + prefix_str.size())
    {
        return Error::ERROR;
    }

    // Detect address family and validate prefix.
    in_addr addr4{};
    in6_addr addr6{};

    if (inet_pton(AF_INET, address.c_str(), &addr4) == 1)
    {
        if (parsed_prefix > 32)
        {
            return Error::ERROR;
        }
    }
    else if (inet_pton(AF_INET6, address.c_str(), &addr6) == 1)
    {
        if (parsed_prefix > 128)
        {
            return Error::ERROR;
        }
    }
    else
    {
        return Error::ERROR;
    }

    ip = std::move(address);
    prefix_length = static_cast<uint8_t>(parsed_prefix);

    return Error::SUCCESS;
}

}


class WireGuard
{
    public:
    struct KeyPair
    {
        std::array<uint8_t, 32> private_key{};
        std::array<uint8_t, 32> public_key{};
    
        std::string private_key_base64;
        std::string public_key_base64;

        static Error generate(KeyPair& keys)
        {
            // Generate a cryptographically secure private key.
            if (RAND_bytes(
                    keys.private_key.data(),
                    keys.private_key.size()) != 1)
            {
                std::cerr << "Failed to generate private key\n";
                return Error::ERROR;
            }

            // Create an X25519 private key.
            EVP_PKEY* raw_key = EVP_PKEY_new_raw_private_key(
                EVP_PKEY_X25519,
                nullptr,
                keys.private_key.data(),
                keys.private_key.size()
            );

            if (!raw_key)
            {
                std::cerr << "Failed to create X25519 private key\n";
                return Error::ERROR;
            }

            std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>
                private_key(raw_key, EVP_PKEY_free);

            // Derive the public key.
            size_t public_key_len = keys.public_key.size();

            if (EVP_PKEY_get_raw_public_key(
                    private_key.get(),
                    keys.public_key.data(),
                    &public_key_len) != 1 ||
                public_key_len != keys.public_key.size())
            {
                std::cerr << "Failed to derive public key\n";
                return Error::ERROR;
            }

            // Generate Base64 representations.
            keys.private_key_base64 = encode_base64(keys.private_key);
            keys.public_key_base64 = encode_base64(keys.public_key);

            if (keys.private_key_base64.empty() ||
                keys.public_key_base64.empty())
            {
                std::cerr << "Failed to encode keys\n";
                return Error::ERROR;
            }

            return Error::SUCCESS;
        }
    };

    // TODO move this to the lower public section once we dump this to cpp files
    // struct AllowedIP
    // {
    //     std::string address;
    //     uint8_t prefix_length;
    // };

    private:
    struct MNLSocketCloser
    {
        void operator()(mnl_socket* p) const
        {
            if (p)
            {
                mnl_socket_close(p);
            }
        }
    };

    using MNLSocketPtr = std::unique_ptr<mnl_socket, MNLSocketCloser>;

    void log_error(const std::string& msg)
    {
        std::cerr << "error on interface: " << std::quoted(ifname_) << ". " << msg << ": " << std::strerror(errno) << '\n';
    }

    static std::string encode_base64(const std::array<uint8_t, 32>& key)
    {
        std::array<unsigned char, 45> output{};

        int len = EVP_EncodeBlock(
            output.data(),
            key.data(),
            key.size()
        );

        if (len != 44)
            return {};

        return std::string(
            reinterpret_cast<char*>(output.data()),
            len
        );
    }

    static Error decode_base64(const std::string& encoded, std::array<uint8_t, 32>& output)
    {
        if (encoded.size() != 44 || encoded.back() != '=')
            return Error::ERROR;

        std::array<unsigned char, 33> decoded{};

        int len = EVP_DecodeBlock(
            decoded.data(),
            reinterpret_cast<const unsigned char*>(encoded.data()),
            encoded.size()
        );

        if (len != 33)
            return Error::ERROR;

        // Base64 padding represents the final unused byte.
        if (decoded[32] != 0)
            return Error::ERROR;

        std::copy_n(decoded.begin(), 32, output.begin());

        // Ensure canonical Base64 encoding.
        return (encode_base64(output) == encoded) ? Error::SUCCESS : Error::ERROR;
    }

    template <int bus>
    MNLSocketPtr& get_mnl_socket_and_bind()
    {
        auto open_and_bind = [this](int b) -> MNLSocketPtr
        {
            auto ret = MNLSocketPtr(mnl_socket_open(b));
            if (!ret)
            {
                log_error("mnl_socket_open");
                return std::move(MNLSocketPtr());
            }
            
            if (mnl_socket_bind(ret.get(), 0, MNL_SOCKET_AUTOPID) < 0)
            {
                log_error("mnl_socket_bind");
                return std::move(MNLSocketPtr());
            }

            // could open AND bind
            return std::move(ret);
        };

        if constexpr(NETLINK_GENERIC == bus)
        {
            if (!generic_nl_)
            {
                generic_nl_ = open_and_bind(bus);
            }
            return generic_nl_;
        }
        else if constexpr(NETLINK_ROUTE == bus)
        {
            if (!route_nl_)
            {
                route_nl_ = open_and_bind(bus);
            }
            return route_nl_;
        }
        else
        {
            static_assert(false, "Unsupported bus type"); 
            // no return type. cant compile anyways
        }
    }

    unsigned int get_ifindex()
    {
        if (ifname_.empty())
        {
            return 0;
        }

        // check this index still points to the same thing
        unsigned int current = if_nametoindex(ifname_.c_str());

        if (0 == current)
        {
            log_error("if_nametoindex"); // does if_nametoindex support errno?
            ifindex_ = 0;
        }

        if (ifindex_ != current)
        {
            ifindex_ = current;
        }

        return ifindex_;
    }

    uint16_t get_familyid(const MNLSocketPtr& nl)
    {
        if (!nl)
        {
            return 0;
        }

        auto impl = [this, &nl]()
        {
            uint16_t fam_id = 0; // 0 indicates error

            char buf[MNL_SOCKET_BUFFER_SIZE]{};

            nlmsghdr* nlh = mnl_nlmsg_put_header(buf);

            nlh->nlmsg_type = GENL_ID_CTRL;
            nlh->nlmsg_flags = NLM_F_REQUEST;
            nlh->nlmsg_seq = next_sequence();

            genlmsghdr* genl = static_cast<genlmsghdr*>(mnl_nlmsg_put_extra_header(nlh, sizeof(genlmsghdr)));

            genl->cmd = CTRL_CMD_GETFAMILY;
            genl->version = 1;

            mnl_attr_put_strz(nlh, CTRL_ATTR_FAMILY_NAME, WG_GENL_NAME);

            if (mnl_socket_sendto(nl.get(), nlh, nlh->nlmsg_len) < 0)
            {
                log_error("WireGuard family request");
                return fam_id;
            }

            char response[MNL_SOCKET_BUFFER_SIZE];

            ssize_t ret = mnl_socket_recvfrom(nl.get(), response, sizeof(response));

            if (ret < 0)
            {
                log_error("WireGuard family response");
                return fam_id;
            }


            if (mnl_cb_run(
                    response,
                    ret,
                    nlh->nlmsg_seq,
                    mnl_socket_get_portid(nl.get()),
                    family_msg_cb,
                    &fam_id) < 0)
            {
                log_error("WireGuard family resolution");
                fam_id = 0; // unclear if previous function may have set it yet errored
                return fam_id;
            }

            return fam_id;
        };

        if (0 == family_id_)
        {
            family_id_ = impl();
            if (0 == family_id_)
            {
                std::cerr << "error getting wireguard family on interface " << ifname_ << '\n';
            }
        }

        return family_id_;
    }

    uint32_t next_sequence()
    {
        return ++sequence_counter;
    }

    static int family_attr_cb(const nlattr* attr, void* data)
    {
        auto* family_id = static_cast<uint16_t*>(data);

        if (mnl_attr_get_type(attr) == CTRL_ATTR_FAMILY_ID)
        {
            if (mnl_attr_validate(attr, MNL_TYPE_U16) < 0)
            {
                return MNL_CB_ERROR;
            }

            *family_id = mnl_attr_get_u16(attr);
        }

        return MNL_CB_OK;
    }

    static int family_msg_cb(const nlmsghdr* nlh, void* data)
    {
        auto* genl = static_cast<const genlmsghdr*>(mnl_nlmsg_get_payload(nlh));
        return mnl_attr_parse( nlh, sizeof(*genl), family_attr_cb, data);
    }

    static Error add_allowed_ips( nlmsghdr* nlh, const std::vector<std::string>& allowed_ips)
    {
        nlattr* ips = mnl_attr_nest_start(nlh, WGPEER_A_ALLOWEDIPS);

        if (!ips)
        {
            return Error::ERROR;
        }

        for (const auto& cidr_ip : allowed_ips)
        {

            std::string ip;
            uint8_t prefix_length{};

            if (Error::SUCCESS != net::parse_cidr_ip(
                    cidr_ip,
                    ip,
                    prefix_length))
            {
                std::cerr << "Invalid allowed IP: " << cidr_ip << '\n';
                return Error::ERROR;
            }
            
            nlattr* allowed_ip = mnl_attr_nest_start(nlh, 0);

            if (!allowed_ip)
            {
                return Error::ERROR;
            }

            in_addr addr4{};
            in6_addr addr6{};

            if (inet_pton(AF_INET, ip.c_str(), &addr4) == 1)
            {
                mnl_attr_put_u16(
                    nlh, WGALLOWEDIP_A_FAMILY, AF_INET);

                mnl_attr_put(
                    nlh,
                    WGALLOWEDIP_A_IPADDR,
                    sizeof(addr4),
                    &addr4);
            }
            else
            {
                // already validated the ip. it must be ipv6
                if (inet_pton(AF_INET6, ip.c_str(), &addr6))
                {
                    return Error::ERROR;
                }

                mnl_attr_put_u16(nlh, WGALLOWEDIP_A_FAMILY, AF_INET6);

                mnl_attr_put(
                    nlh,
                    WGALLOWEDIP_A_IPADDR,
                    sizeof(addr6),
                    &addr6);
            }

            mnl_attr_put_u8(
                nlh,
                WGALLOWEDIP_A_CIDR_MASK,
                prefix_length);

            mnl_attr_nest_end(nlh, allowed_ip);
        }

        mnl_attr_nest_end(nlh, ips);

        return Error::SUCCESS;
    }

    Error send_and_wait_ack(mnl_socket* nl, nlmsghdr* nlh)
    {
        if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0)
        {
            log_error("mnl_socket_sendto");
            return Error::ERROR;
        }

        char buf[MNL_SOCKET_BUFFER_SIZE]{};
        unsigned int portid = mnl_socket_get_portid(nl);
        ssize_t ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
        if (ret < 0)
        {
            log_error("mnl_socket_recvfrom");
            return Error::ERROR;
        }

        // Verify the kernel acknowledged without an error code
        if (mnl_cb_run(buf, ret, nlh->nlmsg_seq, portid, nullptr, nullptr) < 0)
        {
            log_error("Netlink error");
            return Error::ERROR;
        }

        return Error::SUCCESS;
    }

public:

    static bool exists(const std::string& ifname)
    {
        return (if_nametoindex(ifname.c_str()) != 0);
    }

    WireGuard() {}

    WireGuard(const std::string& iface_name) : ifname_{iface_name}
    {}

    WireGuard(WireGuard&& rhs) noexcept
{
    std::lock_guard lock(rhs.mutex);

    route_nl_ = std::move(rhs.route_nl_);
    generic_nl_ = std::move(rhs.generic_nl_);
    family_id_ = rhs.family_id_;
    ifname_ = std::move(rhs.ifname_);
    ifindex_ = rhs.ifindex_;
    owns_interface_ = rhs.owns_interface_;
    sequence_counter = rhs.sequence_counter;

    rhs.ifindex_ = 0;
    rhs.family_id_ = 0;
    rhs.owns_interface_ = false;
    rhs.sequence_counter = 0;
}

WireGuard& operator=(WireGuard&& rhs) noexcept
{
    if (this == &rhs)
    {
        return *this;
    }

    std::scoped_lock lock(mutex, rhs.mutex);

    route_nl_ = std::move(rhs.route_nl_);
    generic_nl_ = std::move(rhs.generic_nl_);

    family_id_ = rhs.family_id_;

    ifname_ = std::move(rhs.ifname_);
    ifindex_ = rhs.ifindex_;

    owns_interface_ = rhs.owns_interface_;
    sequence_counter = rhs.sequence_counter;

    // Leave rhs in a harmless moved-from state.
    rhs.ifindex_ = 0;
    rhs.family_id_ = 0;
    rhs.owns_interface_ = false;
    rhs.sequence_counter = 0;

    return *this;
}

    ~WireGuard()
    {
        if (owns_interface_)
        {
            auto previous_ifindex = ifindex_;
            auto ifindex = get_ifindex();
            if (0 == ifindex)
            {
                return; // Error::ERROR;
            }

            // check if this interface got destroyed and recreated from underneath us
            if (previous_ifindex == ifindex)
            {
                destroy();
            }
            // else
            // {
            //     std::cerr << "interface index changed from underneath us. not destroying\n";
            // }
        }
    }

    // can only set the name if its empty or else well have to invalid all our internal records
    Error set_ifname(const std::string& iface_name) 
    {
        if (iface_name.empty() || !ifname_.empty())
        {
            return Error::ERROR;
        }

        ifname_ = iface_name;
        return Error::SUCCESS;

    }

    Error create()
    {
        std::lock_guard lock(mutex);

        auto & nl = get_mnl_socket_and_bind<NETLINK_ROUTE>();
        if (!nl)
        {
            return Error::ERROR;
        }

        // this buffer holds all the data and headers we will request the kernel with.
        // we will fill that data by getting some pointers in the middle of it and casting those to 
        // the appropriate structs
        char buf[MNL_SOCKET_BUFFER_SIZE]{};


        nlmsghdr* nlh = mnl_nlmsg_put_header(buf);
        nlh->nlmsg_type = RTM_NEWLINK;
        // NLM_F_CREATE: Create if it doesn't exist
        // NLM_F_EXCL: Fail if it already exists
        // NLM_F_ACK: Request an acknowledgment from the kernel
        nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
        nlh->nlmsg_seq = next_sequence();
        

        ifinfomsg* ifm = static_cast<ifinfomsg*>(mnl_nlmsg_put_extra_header(nlh, sizeof(ifinfomsg)));
        ifm->ifi_family = AF_UNSPEC;

        mnl_attr_put_strz(nlh, IFLA_IFNAME, ifname_.c_str());

        nlattr* linkinfo = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
        mnl_attr_put_strz(nlh, IFLA_INFO_KIND, "wireguard");
        mnl_attr_nest_end(nlh, linkinfo);

        auto result = send_and_wait_ack(nl.get(), nlh);
        if (Error::SUCCESS != result)
        {
            return result;
        }

        owns_interface_ = true; // i think this is now true regardless of ifindex

        auto ifindex = get_ifindex();
        if (0 == ifindex)
        {
            return Error::ERROR;
        }

        return Error::SUCCESS;
    }

    Error destroy()
    {
        std::lock_guard lock(mutex);

        auto ifindex = get_ifindex();
        
        // cant destroy something that was never constructed or already destroyed
        if (ifindex == 0 || ifname_.empty())
        {
            return Error::SUCCESS;
        }

        auto & nl = get_mnl_socket_and_bind<NETLINK_ROUTE>();
        if (!nl) 
        {
            return Error::ERROR;
        }

        char buf[MNL_SOCKET_BUFFER_SIZE]{};
        nlmsghdr* nlh = mnl_nlmsg_put_header(buf);
        nlh->nlmsg_type = RTM_DELLINK;
        nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
        nlh->nlmsg_seq = next_sequence();

        ifinfomsg* ifm = static_cast<ifinfomsg*>(mnl_nlmsg_put_extra_header(nlh, sizeof(ifinfomsg)));
        ifm->ifi_family = AF_UNSPEC;
        ifm->ifi_index = ifindex;

        auto result = send_and_wait_ack(nl.get(), nlh);
        if (Error::SUCCESS != result)
        {
            return result;
        }

        // wipe our internal records of the interface
        ifindex_ = 0; 
        owns_interface_ = false;
        family_id_ = 0;

        return result;
    }

    Error set_private_key(const std::string& private_key_base64)
{
    std::lock_guard lock(mutex);

    // Find interface index.
    auto ifindex = get_ifindex();
    if (0 == ifindex)
    {
        return Error::ERROR;
    }

    // Decode private key.
    std::array<uint8_t, 32> private_key_bytes{};

    if (Error::SUCCESS != decode_base64(private_key_base64, private_key_bytes))
    {
        std::cerr << "Invalid private key\n";
        return Error::ERROR;
    }

    // Create Generic Netlink socket.
    auto & nl = get_mnl_socket_and_bind<NETLINK_GENERIC>();
    if (!nl)
    {
        return Error::ERROR;
    }

    // Resolve WireGuard Generic Netlink family.
    auto family_id = get_familyid(nl);
    if (0 == family_id)
    {
        return Error::ERROR;
    }

    // Construct Generic Netlink request.
    char buf[MNL_SOCKET_BUFFER_SIZE]{};

    nlmsghdr* nlh = mnl_nlmsg_put_header(buf);

    nlh->nlmsg_type = family_id;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    nlh->nlmsg_seq = next_sequence();

    genlmsghdr* genl = static_cast<genlmsghdr*>(mnl_nlmsg_put_extra_header(nlh, sizeof(genlmsghdr)));

    genl->cmd = WG_CMD_SET_DEVICE;
    genl->version = WG_GENL_VERSION;

    // Identify interface.
    mnl_attr_put_u32(nlh, WGDEVICE_A_IFINDEX, ifindex);

    // Set private key.
    mnl_attr_put(
        nlh,
        WGDEVICE_A_PRIVATE_KEY,
        private_key_bytes.size(),
        private_key_bytes.data()
    );

    // Send request.
    Error result = send_and_wait_ack(nl.get(), nlh);

    // Clear temporary private key material.
    OPENSSL_cleanse(
        private_key_bytes.data(),
        private_key_bytes.size()
    );

    return result;
}

    Error set_listen_port(uint16_t port)
    {
        std::lock_guard lock(mutex);

        auto ifindex = get_ifindex();
        if (0 == ifindex)
        {
            return Error::ERROR;
        }

        auto & nl = get_mnl_socket_and_bind<NETLINK_GENERIC>();
        if (!nl)
        {
            return Error::ERROR;
        }

        auto family_id = get_familyid(nl);
        if (0 == family_id)
        {
            return Error::ERROR;
        }

        char buf[MNL_SOCKET_BUFFER_SIZE]{};

        nlmsghdr* nlh = mnl_nlmsg_put_header(buf);

        nlh->nlmsg_type = family_id;
        nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
        nlh->nlmsg_seq = next_sequence();

        genlmsghdr* genl =
            static_cast<genlmsghdr*>(
                mnl_nlmsg_put_extra_header(nlh, sizeof(genlmsghdr))
            );

        genl->cmd = WG_CMD_SET_DEVICE;
        genl->version = WG_GENL_VERSION;

        // Identify the interface
        mnl_attr_put_u32(nlh, WGDEVICE_A_IFINDEX, ifindex);

        // Set the listening UDP port
        mnl_attr_put_u16(nlh, WGDEVICE_A_LISTEN_PORT, port);

        return send_and_wait_ack(nl.get(), nlh);
    }

    Error set_ip(const std::string& ip, uint8_t prefix_length)
    {
        std::lock_guard lock(mutex);

        auto ifindex = get_ifindex();
        if (0 == ifindex)
        {
            return Error::ERROR;
        }

        // Detect address family.
        int family = AF_UNSPEC;
        const void* address = nullptr;
        size_t address_size = 0;

        in_addr addr4{};
        in6_addr addr6{};

        if (inet_pton(AF_INET, ip.c_str(), &addr4) == 1)
        {
            family = AF_INET;
            address = &addr4;
            address_size = sizeof(addr4);

            if (prefix_length > 32)
            {
                std::cerr << "Invalid IPv4 prefix length\n";
                return Error::ERROR;
            }
        }
        else if (inet_pton(AF_INET6, ip.c_str(), &addr6) == 1)
        {
            family = AF_INET6;
            address = &addr6;
            address_size = sizeof(addr6);

            if (prefix_length > 128)
            {
                std::cerr << "Invalid IPv6 prefix length\n";
                return Error::ERROR;
            }
        }
        else
        {
            std::cerr << "Invalid IP address: " << ip << '\n';
            return Error::ERROR;
        }

        // Create Route Netlink socket.
        auto& nl = get_mnl_socket_and_bind<NETLINK_ROUTE>();
        if (!nl)
        {
            return Error::ERROR;
        }

        // Construct Netlink request.
        char buf[MNL_SOCKET_BUFFER_SIZE]{};

        nlmsghdr* nlh = mnl_nlmsg_put_header(buf);

        nlh->nlmsg_type = RTM_NEWADDR;
        nlh->nlmsg_flags =
            NLM_F_REQUEST |
            NLM_F_CREATE |
            NLM_F_REPLACE |
            NLM_F_ACK;

        nlh->nlmsg_seq = next_sequence();

        ifaddrmsg* ifa =
            static_cast<ifaddrmsg*>(
                mnl_nlmsg_put_extra_header(nlh, sizeof(ifaddrmsg))
            );

        ifa->ifa_family = family;
        ifa->ifa_prefixlen = prefix_length;
        ifa->ifa_flags = IFA_F_PERMANENT;
        ifa->ifa_scope = RT_SCOPE_UNIVERSE;
        ifa->ifa_index = ifindex;

        // Assign the address.
        mnl_attr_put(
            nlh,
            IFA_LOCAL,
            address_size,
            address
        );

        mnl_attr_put(
            nlh,
            IFA_ADDRESS,
            address_size,
            address
        );

        return send_and_wait_ack(nl.get(), nlh);
    }

    Error set_ip(const std::string& endpoint)
    {
        std::string ip{};
        uint8_t prefix_length{};
        if (Error::SUCCESS != net::parse_cidr_ip(endpoint, ip, prefix_length))
        {
            return Error::ERROR;
        }

        return set_ip(ip, prefix_length);
    }

    // Bring the interface UP (equivalent to `ip link set dev <ifname> up`)
    Error set_up_down(bool up)
    {
        std::lock_guard lock(mutex);

        auto ifindex = get_ifindex();
        if (0 == ifindex)
        {
            return Error::ERROR;
        }

        auto & nl = get_mnl_socket_and_bind<NETLINK_ROUTE>();
        if (!nl)
        {
            return Error::ERROR;
        }

        char buf[MNL_SOCKET_BUFFER_SIZE]{};
        nlmsghdr* nlh = mnl_nlmsg_put_header(buf);
        nlh->nlmsg_type = RTM_NEWLINK;
        nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
        nlh->nlmsg_seq = next_sequence();

        ifinfomsg* ifm = static_cast<ifinfomsg*>(mnl_nlmsg_put_extra_header(nlh, sizeof(ifinfomsg)));
        ifm->ifi_family = AF_UNSPEC;
        ifm->ifi_index = ifindex;
        ifm->ifi_flags = (up ? IFF_UP : 0);   // Desired state
        ifm->ifi_change = IFF_UP;  // Bitmask of flags we are changing

        return send_and_wait_ack(nl.get(), nlh);
    }
    
    Error add_peer(const std::string& public_key, const std::vector<std::string>& allowed_ips, const std::string& public_ip, uint16_t port)
    {
        std::lock_guard lock(mutex);

        // Find interface index.
        auto ifindex = get_ifindex();
        if (0 == ifindex)
        {
            return Error::ERROR;
        }

        // Validate endpoint configuration.
        if (!public_ip.empty() && port == 0)
        {
            std::cerr << "Invalid endpoint port\n";
            return Error::ERROR;
        }

        // Decode the peer's public key.
        std::array<uint8_t, 32> public_key_bytes{};

        if (Error::SUCCESS != decode_base64(public_key, public_key_bytes))
        {
            std::cerr << "Invalid WireGuard public key\n";
            return Error::ERROR;
        }

        // Create a Generic Netlink socket.
        auto & nl = get_mnl_socket_and_bind<NETLINK_GENERIC>();
        if (!nl)
        {
            return Error::ERROR;
        }

        // Resolve WireGuard Generic Netlink family.
        auto family_id = get_familyid(nl);
        if (0 == family_id)
        {
            return Error::ERROR;
        }

        // Construct the endpoint sockaddr, if provided.
        sockaddr_storage endpoint{};
        socklen_t endpoint_len = 0;

        if (!public_ip.empty())
        {
            auto* endpoint4 = reinterpret_cast<sockaddr_in*>(&endpoint);
            auto* endpoint6 = reinterpret_cast<sockaddr_in6*>(&endpoint);

            if (inet_pton(AF_INET, public_ip.c_str(), &endpoint4->sin_addr) == 1)
            {
                endpoint4->sin_family = AF_INET;
                endpoint4->sin_port = htons(port);
                endpoint_len = sizeof(sockaddr_in);
            }
            else if (inet_pton(AF_INET6, public_ip.c_str(), &endpoint6->sin6_addr) == 1)
            {
                endpoint6->sin6_family = AF_INET6;
                endpoint6->sin6_port = htons(port);
                endpoint_len = sizeof(sockaddr_in6);
            }
            else
            {
                std::cerr << "Invalid endpoint address: " << public_ip << '\n';
                return Error::ERROR;
            }
        }

        // Construct Generic Netlink request.
        char buf[MNL_SOCKET_BUFFER_SIZE]{};

        nlmsghdr* nlh = mnl_nlmsg_put_header(buf);
        nlh->nlmsg_type = family_id;
        nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
        nlh->nlmsg_seq = next_sequence();

        genlmsghdr* genl = static_cast<genlmsghdr*>(
            mnl_nlmsg_put_extra_header(nlh, sizeof(genlmsghdr)));

        genl->cmd = WG_CMD_SET_DEVICE;
        genl->version = WG_GENL_VERSION;

        // Identify the WireGuard interface.
        mnl_attr_put_u32(nlh, WGDEVICE_A_IFINDEX, ifindex);

        // Start peers container.
        nlattr* peers = mnl_attr_nest_start(nlh, WGDEVICE_A_PEERS);

        if (!peers)
        {
            std::cerr << "Failed to create peers attribute\n";
            return Error::ERROR;
        }

        // Start individual peer.
        nlattr* peer = mnl_attr_nest_start(nlh, 0);

        if (!peer)
        {
            std::cerr << "Failed to create peer attribute\n";
            return Error::ERROR;
        }

        // Peer public key.
        mnl_attr_put(
            nlh,
            WGPEER_A_PUBLIC_KEY,
            public_key_bytes.size(),
            public_key_bytes.data());

        // Peer endpoint (optional).
        if (!public_ip.empty())
        {
            mnl_attr_put(
                nlh,
                WGPEER_A_ENDPOINT,
                endpoint_len,
                &endpoint);
        }

        // Configure allowed IPs.
        if (Error::SUCCESS != add_allowed_ips(nlh, allowed_ips))
        {
            std::cerr << "Failed to configure allowed IPs\n";
            return Error::ERROR;
        }

        // Close nested attributes.
        mnl_attr_nest_end(nlh, peer);
        mnl_attr_nest_end(nlh, peers);

        // Send request and wait for kernel ACK.
        return send_and_wait_ack(nl.get(), nlh);
    }


    Error add_peer(const std::string& public_key, const std::vector<std::string>& allowed_ips, const std::string& public_endpoint)
    {
        std::string ip{};
        uint16_t port{};

        if (Error::SUCCESS != net::parse_endpoint(public_endpoint, ip, port))
        {
            return Error::ERROR;
        }

        return add_peer(public_key, allowed_ips, ip, port);
    }

    Error add_peer(const std::string& public_key, const std::vector<std::string>& allowed_ips)
    {
        return add_peer(public_key, allowed_ips, "", 0);
    }

    Error remove_peer(const std::string& public_key)
    {
        std::lock_guard lock(mutex);

        // Find interface index.
        auto ifindex = get_ifindex();
        if (0 == ifindex)
        {
            return Error::ERROR;
        }

        // Decode peer public key.
        std::array<uint8_t, 32> public_key_bytes{};

        if (Error::SUCCESS != decode_base64(public_key, public_key_bytes))
        {
            std::cerr << "Invalid WireGuard public key\n";
            return Error::ERROR;
        }

        // Create Generic Netlink socket.
        auto & nl = get_mnl_socket_and_bind<NETLINK_GENERIC>();
        if (!nl)
        {
            return Error::ERROR;
        }

        // Resolve WireGuard Generic Netlink family.
        auto family_id = get_familyid(nl);
        if (0 == family_id)
        {
            return Error::ERROR;
        }

        // Construct Generic Netlink request.
        char buf[MNL_SOCKET_BUFFER_SIZE]{};

        nlmsghdr* nlh = mnl_nlmsg_put_header(buf);

        nlh->nlmsg_type = family_id;
        nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
        nlh->nlmsg_seq = next_sequence();

        genlmsghdr* genl = static_cast<genlmsghdr*>(
            mnl_nlmsg_put_extra_header(nlh, sizeof(genlmsghdr))
        );

        genl->cmd = WG_CMD_SET_DEVICE;
        genl->version = WG_GENL_VERSION;

        // Identify interface.
        mnl_attr_put_u32(
            nlh,
            WGDEVICE_A_IFINDEX,
            ifindex
        );

        // Start peers container.
        nlattr* peers = mnl_attr_nest_start(nlh, WGDEVICE_A_PEERS);
        if (!peers)
        {
            std::cerr << "Failed to create peers attribute\n";
            return Error::ERROR;
        }

        // Start peer.
        nlattr* peer = mnl_attr_nest_start(nlh, 0);
        if (!peer)
        {
            std::cerr << "Failed to create peer attribute\n";
            return Error::ERROR;
        }

        // Identify peer by public key.
        mnl_attr_put(
            nlh,
            WGPEER_A_PUBLIC_KEY,
            public_key_bytes.size(),
            public_key_bytes.data()
        );

        // Mark peer for removal.
        mnl_attr_put_u32(
            nlh,
            WGPEER_A_FLAGS,
            WGPEER_F_REMOVE_ME
        );

        // Close nested attributes.
        mnl_attr_nest_end(nlh, peer);
        mnl_attr_nest_end(nlh, peers);

        // Send request.
        return send_and_wait_ack(nl.get(), nlh);
    }

private:

    MNLSocketPtr route_nl_{};
    MNLSocketPtr generic_nl_{};

    uint16_t family_id_{};

    std::string ifname_{};
    unsigned int ifindex_{};

    bool owns_interface_{};
    int sequence_counter{};

    std::mutex mutex{};
};

}

#endif


// WireGuard::add_peer(
//     "wg0",
//     "BASE64_PUBLIC_KEY_HERE",
//     "203.0.113.10",
//     51820
// );