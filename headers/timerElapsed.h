#ifndef TIMER_ELAPSED
#define TIMER_ELAPSED

#include <chrono>

class TimerElapsed{
private:
    using clock = std::chrono::steady_clock;

    clock::time_point _start;

public:
    TimerElapsed() : _start(clock::now()){}

    void reset(){
        _start = clock::now();
    }

    double elapsed_ms() const{
        return std::chrono::duration<double, std::milli>(
            clock::now() - _start)
        .count();
    }

};

#endif