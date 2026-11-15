#ifndef COMMON_SIGNAL_HPP
#define COMMON_SIGNAL_HPP

// #include <semaphore>
#include <stop_token>
#include "common/utils/singleton.hpp"
#include <iostream>

namespace common::utils {

struct SignalReceiver
{
    int last_signal{};
    std::stop_source stop_source{};

    void signal(int signal)
    {
        std::cout << "got signal: " << signal << '\n';
        last_signal = signal;
        stop_source.request_stop();
    }

    std::stop_token get_token()
    {
        return stop_source.get_token();
    }

    void request_stop()
    {
        std::cout << "stop requested\n";
        stop_source.request_stop();
    }
};

using GlobalSignalHandler = Singleton<SignalReceiver>;

}

#endif