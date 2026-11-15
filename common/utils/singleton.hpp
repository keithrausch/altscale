#ifndef COMMON_SINGLETON_HPP
#define COMMON_SINGLETON_HPP

namespace common::utils {

    // TODO mark that T cannot be copied or moved by Requires clause

template <typename T>
class Singleton {
public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

    static T& instance() {
        static T instance;
        return instance;
    }

protected:
    Singleton() = default;
    virtual ~Singleton() = default;
};

}
#endif