#ifndef COMMON_MANAGER_BASE_HPP
#define COMMON_MANAGER_BASE_HPP

#include <atomic> 
#include <thread>
#include <mutex>
#include <memory>
#include <set>
#include <functional>
#include <stop_token>
#include <unordered_map>
#include "common/utils/singleton.hpp"
#include "common/utils/xml.hpp"

namespace common::managers {

class ManagerBase
{
    
    public:
    std::string name{};
    std::string type{};

    std::atomic<bool> running{};
    using StopCallbackT = std::stop_callback<std::function<void(void)>>;

    ManagerBase(const std::string& name_in, const std::string& type_in) : name{name_in}, type{type_in}
    {
        register_stop_callback([this](){running = false;});
    }

    virtual void read_xml(common::utils::XML& xml) = 0;
    virtual void process() = 0;
    
    void request_stop()
    {
        stop_source.request_stop();
    }

    std::stop_token get_stop_token()
    {
        return stop_source.get_token();
    }

    template <typename CallbackT>
    void register_stop_callback(const CallbackT& callback)
    {
        auto callback_ptr = std::make_unique<StopCallbackT>(stop_source.get_token(), callback);
        if (callback_ptr)
        {
            stop_callbacks.push_back(std::move(callback_ptr));
        }
    }

    private:

    std::stop_source stop_source{};
    std::vector<std::unique_ptr<StopCallbackT>> stop_callbacks{};
};


class ManagerRegisteryImpl
{
    public:

    using ManagerFactoryFuncT = std::function<std::shared_ptr<ManagerBase>(std::string, std::string)>;

    template <typename ManagerT>
    bool register_manager(const std::string& manager_type)
    {
        std::lock_guard lock(mutex);

        bool had_error = false;
        had_error |= manager_factories.contains(manager_type);

        if (!had_error)
        {
            manager_factories[manager_type] = [](const std::string& name, const std::string& type){return std::make_shared<ManagerT>(name, type);};
        }
        else
        {
            std::cerr << "type already registered: " << std::quoted(manager_type) << '\n';
        }

        return had_error;
    }

    ManagerFactoryFuncT get_factory_by_type(const std::string& manager_type)
    {
        std::lock_guard lock(mutex);

        if (manager_factories.contains(manager_type))
        {
            auto& factory = manager_factories.at(manager_type);
            return factory;
        }

        return ManagerFactoryFuncT();

    }

    std::set<std::string> get_all_types()
    {
        std::lock_guard lock(mutex);

        std::set<std::string> all_types{};

        for (const auto & [name, factory] : manager_factories)
        {
            all_types.insert(name);
        }

        return all_types;
    }
    private:
    std::mutex mutex{};
    std::unordered_map<std::string, ManagerFactoryFuncT> manager_factories{};

};

using ManagerRegistry = common::utils::Singleton<ManagerRegisteryImpl>;

template <typename ManagerT>
struct ManagerRegisterer
{
    ManagerRegisterer(const std::string& manager_type)
    {
        ManagerRegistry::instance().register_manager<ManagerT>(manager_type);
    }
};

}
#endif