#ifndef GLOBALTIMER_HPP
#define GLOBALTIMER_HPP
#include <cstdint>
#include <chrono>

class global_timer
{
public:
    using rep = int64_t;
    using period = std::ratio<2, 666666687>;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<global_timer>;

    static time_point now();
private:
};

#endif // GLOBALTIMER_HPP
