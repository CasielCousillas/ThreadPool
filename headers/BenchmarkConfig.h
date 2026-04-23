#ifndef BENCHMARKCONFIG
#define BENCHMARKCONFIG

#include <functional>
#include <thread>
#include <atomic>
#include <vector>

struct BenchmarkConfig{
    int producer_threads;
    int consumer_threads;
    int seconds_duration;

    enum class TaskType{
        Light,
        Medium,
        Heavy
    } task_type;

    BenchmarkConfig(const TaskType& type, int seconds, int producer_count = 5, int consumer_count = 5) : 
        task_type(type), seconds_duration(seconds), producer_threads(producer_count), consumer_threads(consumer_count){}

};

struct Metrics{
    struct counter{
        std::atomic<long long> value = 0;
    };

    std::atomic<long long> total_tasks = 0;
    std::vector<counter> task_per_thread;

    Metrics(const size_t& n) : task_per_thread(n){}
};

template<typename T>
class IQueue {
public:
    virtual void push(T) = 0;
    virtual bool pop(T&) = 0;
    virtual void close() = 0;
    virtual ~IQueue() = default;
};

std::function<void()> make_task(BenchmarkConfig::TaskType type) {
    switch(type) {
        case BenchmarkConfig::TaskType::Light:
            return [] { asm volatile(""); };

        case BenchmarkConfig::TaskType::Medium:
            return [] {
                for (int i = 0; i < 1000; ++i);
            };

        case BenchmarkConfig::TaskType::Heavy:
            return [] {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            };
    }

    return []{};
}

template<typename T>
void make_run(IQueue<T>& q, const BenchmarkConfig& cfg){
    Metrics metrics(cfg.consumer_threads);
    std::atomic<bool> running = true;

    std::vector<std::thread> threads; //! Deberia de presetear el tamanio

    //Consumers
    for(int i = 0; i < cfg.consumer_threads; i++){
        threads.emplace_back([&, i]{
            std::function<void()> f;
            while(q.pop(f)){
                f();
                metrics.total_tasks++;
                metrics.task_per_thread[i].value++;
            }
        });
    }

    //Productores
    for(int i = 0; i < cfg.producer_threads; i++){
        threads.emplace_back([&]{
            std::function<void()> task = make_task(cfg.task_type);
            while(running){
                q.push(task);
            }
        });
    }

    //Run
    std::this_thread::sleep_for(std::chrono::seconds(cfg.seconds_duration));
    running = false;

    //Close
    q.close();

    //Results
    long long total = metrics.total_tasks.load();
    double throughput = total / (double)cfg.seconds_duration;

    std::cout << "\n=== RESULT ===\n";
    std::cout << "Total tasks: " << total << "\n";
    std::cout << "Throughput: " << throughput << " ops/sec\n";

    std::cout << "\nFairness:\n";
    for (int i = 0; i < cfg.consumer_threads; i++) {
        auto v = metrics.task_per_thread[i].value.load();
        double pct = (100.0 * v) / total;
        std::cout << "Thread " << i << ": " << pct << "%\n";
    }
}

#endif