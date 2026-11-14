#ifndef SLEEPER_HPP
#define SLEEPER_HPP

#include <mutex>
#include <condition_variable>

class Sleeper
{
    std::mutex mutex{};
    std::condition_variable cv{};
    bool awake{};

    public:
    void sleep()
    {
        std::unique_lock lock(mutex);
        cv.wait(lock, [this]() { return awake; });
        awake = false;
    }

    template <typename DurationT>
    bool sleep_for(const DurationT& duration)
    {
        std::unique_lock lock(mutex);
        awake = false;

        bool woken_by_user = cv.wait_for(lock, duration, [this](){return awake;});
        if (woken_by_user)
        {
            awake = false;
        }

        return woken_by_user;

    }

    void wake()
    {
        {
            std::unique_lock lock(mutex);
            awake = true;
        }

        cv.notify_all();
    }
};


#endif