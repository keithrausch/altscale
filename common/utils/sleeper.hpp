#ifndef COMMON_UTILS_SLEEPER_HPP
#define COMMON_UTILS_SLEEPER_HPP

#include <mutex>
#include <condition_variable>

namespace common::utils {

class Sleeper
{
    std::mutex mutex{};
    std::condition_variable cv{};
    bool awake_desired{};

    public:
    void sleep()
    {
        std::unique_lock lock(mutex);
        cv.wait(lock, [this]() { return awake_desired; });
        awake_desired = false;
    }

    // return true if woken by wake(). false if it timed out on its own
    template <typename DurationT>
    bool sleep_for(const DurationT& duration)
    {
        std::unique_lock lock(mutex);

        bool last_awake_desired = cv.wait_for(lock, duration, [this](){return awake_desired;});
        awake_desired = false;

        return last_awake_desired;

    }

    void wake()
    {
        {
            std::unique_lock lock(mutex);
            awake_desired = true;
        }

        cv.notify_all();
    }
};

}

#endif