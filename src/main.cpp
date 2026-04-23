#include <iostream>
#include "blockingQueue.h"
#include "pool.h"
#include "timerElapsed.h"
#include "asyncLogger.h"
#include "BenchmarkConfig.h"

// 0.135671 con std::cout
// 0.00185593 con asyncLogger
// 0.001141 con std::stringstream
// 0.00118941 sin nada

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
    BenchmarkConfig cfg(BenchmarkConfig::TaskType::Medium, 1);
    
    BlockingQueue<std::function<void()>> real_queue(INT_MAX);
    MyQueueAdapter<std::function<void()>> queue_adapter(real_queue);

    make_run(queue_adapter, cfg);
}
