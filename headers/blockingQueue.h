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
    std::condition_variable cv_producer;
    std::condition_variable cv_consumer;
    std::queue<T> q;
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

            q.push(std::forward<U>(value));
        }
        cv_consumer.notify_one();
        return true;
    }

    /*
        Try_push garantiza que
        - Devuelve true, el elemento fue insertado en la cola, exactamente una vez
        - Devuelve false, si no hay espacio o esta la cola cerrada; el elemento no fue insertado en la cola
    */
    template<typename U>
    bool try_push(U&& value){
        {
            std::unique_lock<std::mutex> guard(m);
            if(closed || q.size() >= max_size_queue)
                return false;
            
            q.push(std::forward<U>(value));
        }

        cv_consumer.notify_one();
        return true;
    }

    template<typename U, typename Rep, typename Period>
    bool push_timeout(U&& value, const std::chrono::duration<Rep, Period>& timeout){
        {
            std::unique_lock<std::mutex> guard(m);
            auto deadline = std::chrono::steady_clock::now() + timeout;
            
            if(!cv_producer.wait_until(guard, deadline, [this] {return q.size() < max_size_queue || closed;}) 
                || closed){
                    return false;
                }
            
            q.push(std::forward<U>(value));
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
            q.pop();
        }
        cv_producer.notify_one();
        return true;
    }

    /*
        Pop garantiza que:
            - El valor obtenido proviene de un push exitoso previo (FIFO)
            - Devuelve true, si se pudo sacar y asignar un valor de la cola exitosamente
            - Devuelve false, si la cola esta vacia, out queda intacto
    */
    bool try_pop(T& out){
        {
            std::unique_lock<std::mutex> guard(m);
            
            if(q.empty())
                return false;
            
            out = std::move(q.front()); 
            q.pop();
        }
        cv_producer.notify_one();
        return true;
    }

    template<typename Rep, typename Period>
    bool pop(T& out, const std::chrono::duration<Rep, Period>& timeout){
        {
            std::unique_lock<std::mutex> guard(m);
            auto deadline = std::chrono::steady_clock::now() + timeout;

            if(!cv_consumer.wait_until(guard, deadline, [this] {return !q.empty() || closed;}) 
                || q.empty()){
                    return false;
                }
            
            out = std::move(q.front()); 
            q.pop();
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
        cv_producer.notify_all();
        cv_consumer.notify_all();
    }
};

#endif