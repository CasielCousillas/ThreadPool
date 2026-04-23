#include <iostream>
#include "blockingQueue.h"
#include "pool.h"
#include "timerElapsed.h"
#include "asyncLogger.h"

/*
    Tengo N productores, M consumidores, mido:
     - Tareas por segundo
     - Uso de la cpu
*/

template<typename Rep, typename Period>
void throughput(const std::chrono::duration<Rep, Period> time, const int p = 5, const int c = 5){
    BlockingQueue<std::function<void()>> bq(INT_MAX);
    AsyncLogger al;

    const int producer_count = p;
    const int consumer_count = c;
    const int items_per_producer = 100;

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    auto start = std::chrono::high_resolution_clock::now();

    for(int i = 0; i < producer_count; i++){
        producers.emplace_back([&bq, &time, i] {
            for(int j = 0; j < items_per_producer; j++){
                bq.push([&time]{
                    std::this_thread::sleep_for(time);
                });
            }
        });
    }

    for(int i = 0; i < consumer_count; i++){
        consumers.emplace_back([&bq, &al, i] {
            std::function<void()> f;
            int cont = 0;
            while(bq.pop(f)){
                cont++;
                f();
            }
            al.addLog("Consumer ", i, " do : ", cont, " tasks!", '\n');
        });
    }

    for(auto& p : producers)
        p.join();
    
    bq.close();
    
    for(auto& c : consumers)
        c.join();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration<double>(end - start).count();

    al.addLog("Total time: ", dur, '\n');
    al.addLog("Tasks per second: ", (producer_count * items_per_producer)/dur, '\n');

}

// 0.135671 con std::cout
// 0.00185593 con asyncLogger
// 0.001141 con std::stringstream
// 0.00118941 sin nada

int main(){
    throughput(std::chrono::microseconds(10));

}