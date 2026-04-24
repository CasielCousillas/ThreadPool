#include <iostream>
#include "blockingQueue.h"
#include "pool.h"
#include "timerElapsed.h"
#include "asyncLogger.h"
#include "BenchmarkConfig.h"

template<typename T>
class MyQueueAdapter : public IQueue<T> {
private:
    BlockingQueue<T>& q;

public:
    MyQueueAdapter(BlockingQueue<T>& q) : q(q) {}

    void push(T t) override {
        q.push(std::move(t));
    }

    bool pop(T& out) override {
        return q.pop(out);
    }

    void close() override {
        q.close();
    }
};

int main(){
    using task = std::function<void()>;

    for (auto task_type : {
        BenchmarkConfig::TaskType::Light,
        BenchmarkConfig::TaskType::Medium,
        BenchmarkConfig::TaskType::HeavyCPU
    }) {
        BenchmarkConfig cfg;
        BlockingQueue<task> real_queue(INT_MAX);
        MyQueueAdapter<task> queue(real_queue);
        cfg.producers = 1;
        cfg.consumers = 5;
        cfg.seconds_dur = 1;
        cfg.task_type = task_type;

        make_run(queue, cfg);
    }
}
