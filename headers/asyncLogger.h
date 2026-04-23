#ifndef ASYNCLOGGER
#define ASYNCLOGGER

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <deque>
#include <thread>
#include <sstream>

class AsyncLogger{
private:
    std::mutex m;
    std::condition_variable cv;
    std::thread worker;
    std::deque<std::string> d;
    bool closed = false;
    
private:
    void working(){
        std::string log;
        while(true){
            std::unique_lock<std::mutex> guard(m);
            cv.wait(guard, [this] {return !d.empty() || closed;});

            if(closed && d.empty())
                break;
            
            log = std::move(d.front());
            d.pop_front();

            guard.unlock();

            std::cout.write(log.data(), log.size());
        }
    }

public:
    AsyncLogger(){
        worker = std::thread(&AsyncLogger::working, this);
    }
    
    template<typename... T>
    void addLog(const T&... items){
        std::ostringstream oss;
        (oss << ... << items);
        {
            if(closed)
                return;

            std::lock_guard<std::mutex> guard(m);
            d.emplace_back(oss.str());
        }

        cv.notify_one();
    }
    
    void close(){
        {
            std::lock_guard<std::mutex> guard(m);
            if(closed)
                return;
            closed = true;
        }
        
        cv.notify_one();
        worker.join();
    }

    ~AsyncLogger(){
        close();
    }
};

#endif