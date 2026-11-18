#ifndef COMMON_UTILS_MAIN_HPP
#define COMMON_UTILS_MAIN_HPP

#include <filesystem>
#include <csignal>
#include <condition_variable>
#include "common/managers/manager_runner.hpp"
#include "common/utils/signal.hpp"
#include "common/managers/manager_base.hpp"

namespace common::utils {

extern "C" void signal_handler(int signal)
{
    // it is NOT lock-free. not sure i care.
    GlobalSignalHandler::instance().signal(signal);
}

int main_helper(int argc, char* argv[])
{
    if (std::signal(SIGINT, signal_handler) == SIG_ERR)
    {
        std::cerr << "error installing SIGINT handler\n";
    }

    bool had_error = false;

    std::cout << "starting...\n";
    std::cout << "working directory: " << std::filesystem::current_path() << '\n';

    std::cout << "registered manager types:\n";
    {
        auto all_manager_types = common::managers::ManagerRegistry::instance().get_all_types();
        for (const auto & manager_type : all_manager_types)
        {
            std::cout << "  " << manager_type << '\n';
        }
    }
    std::cout << '\n';


    //
    // configuration
    //

    std::filesystem::path config_path = "";
    if (argc >= 2)
    {
        config_path = argv[1];
    }
    
    if (!config_path.empty())
    {
        std::cout << "loading config from " << config_path << '\n';
    }
    else
    {
        std::cerr << "no configuration file provided\n";
        had_error |= true;
    }

    if (had_error)
    {
        return had_error;
    }

    common::managers::ManagerRunner runner{};

    had_error |= runner.load_configs(config_path);
    if (had_error)
    {
        return had_error;
    }

    std::stop_callback stop_callback(GlobalSignalHandler::instance().get_token(), [&runner](){runner.request_stop();});


    //
    // run
    //

    runner.run(GlobalSignalHandler::instance().get_token()); // non blocking

    //
    // shutdown
    //

    {
        std::mutex mutex{};
        std::unique_lock lock(mutex);
        std::condition_variable_any cv;
        cv.wait(lock, GlobalSignalHandler::instance().get_token(), [](){
            return GlobalSignalHandler::instance().get_token().stop_requested();
        });
    }

    std::cout << "exiting\n";

    return (had_error ? 1 : 0);
}

}
#endif