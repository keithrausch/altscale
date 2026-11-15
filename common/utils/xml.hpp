
#ifndef COMMON_XML_HPP
#define COMMON_XML_HPP

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <charconv>
#include <type_traits>
#include <cctype>
#include <iostream>
#include <utility>

#include "rapidxml.hpp"

namespace common::utils {

    class Node
{
public:
    using RapidNode = rapidxml::xml_node<>;

    Node() = default;

    explicit Node(const RapidNode* node)
        : node_(node)
    {
    }

    explicit operator bool() const
    {
        return node_ != nullptr;
    }

    bool valid() const
    {
        return node_ != nullptr;
    }

    std::string_view name() const
    {
        return node_ ? std::string_view(node_->name()) : std::string_view{};
    }

    std::string_view value() const
    {
        return node_ ? std::string_view(node_->value()) : std::string_view{};
    }

    Node child(std::string_view name) const
    {
        if (!node_)
            return {};

        std::string node_name(name);

        return Node{node_->first_node(node_name.c_str())};
    }

    std::vector<Node> children(std::string_view name = {}) const
    {
        std::vector<Node> result;

        if (!node_)
            return result;

        if (name.empty())
        {
            for (auto* child = node_->first_node();
                 child;
                 child = child->next_sibling())
            {
                if (child->type() == rapidxml::node_element)
                    result.emplace_back(child);
            }
        }
        else
        {
            std::string node_name(name);

            for (auto* child = node_->first_node(node_name.c_str());
                 child;
                 child = child->next_sibling(node_name.c_str()))
            {
                result.emplace_back(child);
            }
        }

        return result;
    }

    bool exists(std::string_view path) const
    {
        return find(path).valid();
    }

    Node find(std::string_view path) const
    {
        Node current = *this;

        while (!path.empty())
        {
            auto separator = path.find('.');
            auto part = path.substr(0, separator);

            if (part.empty())
                return {};

            current = current.child(part);

            if (!current)
                return {};

            if (separator == std::string_view::npos)
                break;

            path.remove_prefix(separator + 1);
        }

        return current;
    }

    template<typename T>
    void require(T& destination, std::string_view path) const
    {
        Node node = find(path);

        if (!node)
            throw std::runtime_error(
                "Required XML value not found: " + std::string(path)
            );

        destination = parse_value<T>(node.node_->value());

        // std::cout << path << " <-- " << destination << "\n";
    }

    template<typename T>
    bool optional(T& destination, std::string_view path) const
    {
        Node node = find(path);

        if (!node)
            return false;

        destination = parse_value<T>(node.node_->value());
        return true;
    }

private:
    const RapidNode* node_{};

    // Trim whitespace from both ends.
    static std::string trim(std::string_view value)
    {
        auto begin = value.begin();
        auto end = value.end();

        while (begin != end &&
               std::isspace(static_cast<unsigned char>(*begin)))
            ++begin;

        while (begin != end &&
               std::isspace(static_cast<unsigned char>(*(end - 1))))
            --end;

        return std::string(begin, end);
    }

    // Convert XML text into the requested type.
    template<typename T>
    static T parse_value(const char* raw)
    {
        std::string text = trim(raw ? raw : ""); // TODO trimming all of these doesnt seem wise for strings and std::filesystem::path

        if constexpr (std::is_same_v<T, std::string>)
        {
            return text;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            std::string lower = text;

            for (char& c : lower)
                c = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c))
                );

            if (lower == "true")
                return true;

            if (lower == "false")
                return false;

            // Accept any numeric value as a boolean.
            std::istringstream stream(text);
            long double number{};

            if (!(stream >> number) ||
                (stream >> std::ws, !stream.eof()))
            {
                throw std::runtime_error(
                    "Invalid XML boolean: " + text
                );
            }

            return number != 0;
        }
        else if constexpr (std::is_integral_v<T>)
        {
            T result{};

            auto [ptr, ec] = std::from_chars(
                text.data(),
                text.data() + text.size(),
                result
            );

            if (ec != std::errc{} ||
                ptr != text.data() + text.size())
            {
                throw std::runtime_error(
                    "Invalid XML integer: " + text
                );
            }

            return result;
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            std::istringstream stream(text);
            T result{};

            if (!(stream >> result) ||
                (stream >> std::ws, !stream.eof()))
            {
                throw std::runtime_error(
                    "Invalid XML floating-point value: " + text
                );
            }

            return result;
        }
        else if constexpr (std::is_same_v<T, std::filesystem::path>)
        {
            return std::filesystem::path(text);
        }
        else
        {
            static_assert(
                std::is_same_v<T, void>,
                "Unsupported XML conversion type"
            );
        }
    }
};
class XML
{
public:
    using Node = common::utils::Node;

    XML() = default;

    XML(const XML&) = delete;
    XML& operator=(const XML&) = delete;

    void read(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);

        if (!file)
            throw std::runtime_error(
                "Failed to open XML file: " + path.string()
            );

        std::ostringstream stream;
        stream << file.rdbuf();

        if (file.bad())
            throw std::runtime_error(
                "Failed to read XML file: " + path.string()
            );

        std::string contents = stream.str();

        doc_.clear();
        buffer_.assign(contents.begin(), contents.end());
        buffer_.push_back('\0');

        try
        {
            doc_.parse<0>(buffer_.data());
        }
        catch (const rapidxml::parse_error& e)
        {
            doc_.clear();
            buffer_.clear();


        throw std::runtime_error(
            "XML parsing error in '" + path.string() +
            "': " + e.what());
            // throw;
        }
    }

    Node root() const
    {
        return Node{&doc_};
    }

private:
    std::vector<char> buffer_;
    rapidxml::xml_document<> doc_;
};

}

#endif
