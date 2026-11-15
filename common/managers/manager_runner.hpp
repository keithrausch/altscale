#ifndef COMMON_MANAGER_RUNNER_HPP
#define COMMON_MANAGER_RUNNER_HPP

#include <atomic>
#include "common/managers/manager_base.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <thread>
#include <iomanip>
#include <stop_token>
#include <mutex>

namespace common::managers {

class ManagerRunner
{
    std::mutex mutex{};
    std::unordered_map<std::string, std::shared_ptr<ManagerBase>> managers{};
    std::vector<std::jthread> threads{};
    
    public:
//     bool register_manager(const std::string & name, std::shared_ptr<ManagerBase> manager)
//     {
//         std::lock_guard lock(mutex);
//         bool had_error = false;
//         had_error |= (!manager);
//         had_error |= managers.contains(name);
//         
//         if (!had_error)
//         {
//             managers[name] = manager;
//         }
// 
//         return had_error;
//     }

    void load_configs_impl(const std::filesystem::path& config_path)
    {
        common::utils::XML xml{};
        xml.read(config_path);

        auto node_managers = xml.root().child("managers");

        if (!node_managers)
            throw std::runtime_error("missing <managers> element");

        for (auto node_manager : node_managers.children("manager"))
        {
            bool manager_enabled{};
            std::string manager_name{};
            std::string manager_type{};
            std::string manager_config{};

            node_manager.require(manager_enabled, "enabled");
            node_manager.require(manager_name, "name");
            node_manager.require(manager_type, "type");
            node_manager.require(manager_config, "config");

            auto factory = common::managers::ManagerRegistry::instance().get_factory_by_type(manager_type);

            if (!factory)
            {
                throw std::runtime_error("error finding manager of type \"" + manager_type + "\"");
            }

            auto manager = factory(manager_name, manager_type);

            if (!manager)
            {
                throw std::runtime_error("error instantiating manager of type \"" + manager_type + "\"");
            }

            if (managers.contains(manager_name))
            {
                throw std::runtime_error("manager already exists with name \"" + manager_name + "\"");
            }

            // load config for this manager
            try
            {
                common::utils::XML manager_xml{};
                manager_xml.read(manager_config);
    
                manager->read_xml(manager_xml); // throws, caught by our parent

            }
            catch (const std::exception& e)
            {
                std::cerr << "error parsing " << std::quoted(manager_config) << ". " << e.what() << '\n';
                throw std::runtime_error("manager \""+manager_name+"\" had error while parsing" );
            } 

            managers[manager_name] = manager;
            
        }

    }

    bool load_configs(const std::filesystem::path& config_path)
    {
        bool had_error = false;

        try
        {
            load_configs_impl(config_path);
        }
        catch (const std::exception& e)
        {
            std::cerr << "error parsing " << config_path << ". " << e.what() << '\n';
            had_error |= true;
        } 

        return had_error;
    }

    void run(std::stop_token stop_token)
    {
        auto process_runner = [](const std::string & name, std::shared_ptr<ManagerBase> manager)
        {
            if (!manager)
            {
                return;
            }

            // std::cout << "starting " << std::quoted(name) << '\n';
            manager->process();
            // std::cout << "shutting down " << std::quoted(name) << '\n';
        };

        std::lock_guard lock(mutex);
        for (auto & [name, manager] : managers)
        {
            if (manager)
            {
                manager->running = true;
                threads.emplace_back(process_runner, name, manager);
            }
        }
    }

    void request_stop()
    {
        std::lock_guard lock(mutex);
        for (auto & [name, manager] : managers)
        {
            if (!manager)
            {
                continue;
            }

            manager->request_stop();
        }
    }
};
}
#endif