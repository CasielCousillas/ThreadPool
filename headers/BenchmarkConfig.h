#ifndef BENCHMARKCONFIG
#define BENCHMARKCONFIG

#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <cassert>

struct BenchmarkConfig{
    int producers;
    int consumers;
    int seconds_dur;

    enum class TaskType{
        Light,
        Medium,
        HeavyCPU,
        HeavySleep
    } task_type;

};

struct Metrics{
    /*
        Esto del padding es imporante, ya que, puede haber false sharing sino
    */
    struct alignas(64) PaddingCounted{
        std::atomic<long long> value = 0;
        char padding[64 - sizeof(value)];
    };

    std::atomic<long long> total_tasks = 0;
    std::vector<PaddingCounted> task_per_thread;

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
        // 0.0001ms
        case BenchmarkConfig::TaskType::Light:
            return []{int x = 10;};

        // 0.01ms
        case BenchmarkConfig::TaskType::Medium:
            return []{
                int cont = 0;
                    for (int i = 0; i < 10000; ++i)
                        cont++;
            };
        
        // 1ms (escalabilidad, uso del cpu)
        case BenchmarkConfig::TaskType::HeavyCPU:
            return []{
                int cont = 0;
                    for (int i = 0; i < 700000; ++i)
                        cont++;
            };

        // 1ms (wakeups, latencia)
        case BenchmarkConfig::TaskType::HeavySleep:
            return []{
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            };
        default:
            return []{};
    }

    return []{};
}

template<typename T>
void make_run(IQueue<T>& q, const BenchmarkConfig& cfg){
    Metrics metrics(cfg.consumers);
    std::atomic<bool> running = true;

    std::vector<std::thread> threads; //! Deberia de presetear el tamanio

    //consumerss
    for(int i = 0; i < cfg.consumers; i++){
        threads.emplace_back([&, i]{
            std::function<void()> f;
            while(q.pop(f)){
                if(!running)
                    break;

                f();
                metrics.total_tasks++;
                metrics.task_per_thread[i].value++;
            }
        });
    }

    //Productores
    for(int i = 0; i < cfg.producers; i++){
        threads.emplace_back([&]{
            std::function<void()> task = make_task(cfg.task_type);
            while(running){
                q.push(task);
            }
        });
    }

    //Run
    std::this_thread::sleep_for(std::chrono::seconds(cfg.seconds_dur));
    running = false;

    //Close
    q.close();

    //join
    for(auto& t : threads){
        t.join();
    }

    //Results
    long long total = metrics.total_tasks.load();
    double throughput = total / (double)cfg.seconds_dur;

    std::cout << "\n=== RESULT ===\n";
    std::cout << "Total tasks: " << total << "\n";
    std::cout << "Throughput: " << throughput << " ops/sec\n";

    std::cout << "\nFairness:\n";
    for (int i = 0; i < cfg.consumers; i++) {
        auto v = metrics.task_per_thread[i].value.load();
        double pct = (100.0 * v) / total;
        std::cout << "Thread " << i << ": " << pct << "%\n";
    }
}

#endif