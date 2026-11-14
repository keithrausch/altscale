#ifndef SIGNAL_HPP
#define SIGNAL_HPP

#include <semaphore>


struct SignalReceiver
{
    int last_signal{};
    std::counting_semaphore<1> notifier{0};
};

#endif