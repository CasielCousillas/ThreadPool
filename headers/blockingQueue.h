#ifndef BLOCKING_QUEUE
#define BLOCKING_QUEUE

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

template <typename T>
class BlockingQueue{
private:
    std::mutex m;
    std::condition_variable cv_consumer;
    std::condition_variable cv_producer;
    std::deque<T> q;
    const size_t max_size_queue;
    bool closed = false;

public:
    BlockingQueue(const size_t n) : max_size_queue(n){}

    /*
        Push garantiza que 
        - Devuelve true, el elemento fue insertado en la cola, exactamente una vez
        - Devuelve false, el elemento no fue insertado en la cola
        - Se bloquea, hasta que haya espacio o se cierre la cola
    */
    template<typename U> // Implemento un template para usar perfect forwarding
    bool push(U&& value){
        {
            std::unique_lock<std::mutex> guard(m);
            cv_producer.wait(guard, [this] {return q.size() < max_size_queue || closed;});

            if(closed)
                return false;

            q.push_back(std::forward<U>(value));
        }
        cv_consumer.notify_one();
        return true;
    }

    /*
        Pop garantiza que:
            - El valor obtenido proviene de un push exitoso previo (FIFO)
            - Devuelve true, si se pudo sacar y asignar un valor de la cola exitosamente
            - Devuelve false, si la cola esta cerrada y vacia, out queda intacto
    */
    bool pop(T& out){
        {
            std::unique_lock<std::mutex> guard(m);
            cv_consumer.wait(guard, [this] {return !q.empty() || closed;});
            
            if(closed && q.empty())
                return false;
            
            out = std::move(q.front()); 
            q.pop_front();
        }
        cv_producer.notify_one();
        return true;
    }

    /*
        Close garantiza que:
            - Ningun elemento insertado previamente se pierde
            - Push devuelve False (No se pueden agregar elementos a la cola)
            - Pop continua extrayendo elementos existentes
            - Todos los hilos se despiertan, evitando deadlocks
            - Se puede llamar varias veces (Indepotencia)
    */
    void close(){
        {
            std::lock_guard<std::mutex> guard(m);
            if(closed)
                return;
            closed = true;
        }
        cv_consumer.notify_all();
        cv_producer.notify_all();
    }

    ~BlockingQueue(){
        close();
    }
};

#endif