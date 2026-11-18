// WRITTEN BY AI

#ifndef COMMON_UTILS_FILEIO_HPP
#define COMMON_UTILS_FILEIO_HPP

#include <fstream>
#include <filesystem>
#include <iostream>
#include <optional>

namespace common::utils {

std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    if (file.bad()) {
        return std::nullopt;
    }

    return buffer.str();
}


bool write_file(const std::filesystem::path& path, const std::string& content)
{
    bool had_error = false;

    if (path.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        if (ec)
        {
            std::cerr << "Failed to create directories: " << ec.message() << '\n';
            had_error |= true;
        }
    }

    if (had_error)
    {
        return had_error;
    }

    if (!had_error)
    {
        std::ofstream file(path);
        if (!file)
        {
            std::cerr << "Failed to open file: " << path << '\n';
            had_error |= true;
        }
        else
        {
            file << content;
        }

        had_error |= !file.good();
    }

    return had_error;
}

}
#endif