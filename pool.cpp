#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>
#include <type_traits>

class ThreadPool{
private:
    std::queue<std::function<void()>> q;
    std::vector<std::thread> threads;
    std::mutex m;
    std::condition_variable cv;
    bool closed = false;

private:
    void worker(){
        while(true){
            std::unique_lock<std::mutex> guard(m);
            cv.wait(guard, [this] {return !q.empty() || closed;});
            
            if(closed && q.empty())
                break;

            auto f = std::move(q.front()); 
            q.pop();
            
            guard.unlock();
            f();
        }
    }

public:
    ThreadPool(unsigned int consumer_count){
        for(int i{}; i < consumer_count; i++){
            /* Paso esos parametros, ya que, worker no es una funcion libre
               El thread necesita saber donde esta la funcion y sobre quien ejecutarla
               Por eso le paso el this, intermante la funcion worker es asi void work(ThreadPool* this)*/
            threads.emplace_back(&ThreadPool::worker, this);
        }
    }

    /*
        ! Imporante sobre esta funcion
        1- Uso F&& + Args&&... para aceptar cualquier callable y argumentos,
        preservando perfect forwarding (lvalues/rvalues).

        2- Devuelve std::future<RetType>, donde RetType depende de F, Args...
        Esto permite obtener el resultado de forma asincrónica.

        3- Uso shared_ptr porque la tarea debe sobrevivir después de que addTask termina.
        La lambda en la cola necesita compartir ownership de la task.

        4- Todas las tareas se transforman a void() (type erasure) para poder
        almacenarlas en una cola homogénea.

        5- Creo el std::future asociado a task_ptr, antes del return, para establecer
        donde recibir el resultado, ANTES de ejecutar la tarea
    */
    template<typename F, typename... Args>
    auto addTask(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using RetType = std::invoke_result_t<F, Args...>;

        /*
            ! Tratar de reemplazar este bind con una lambda
        */
         // Bind de la función + argumentos -> ahora es una función sin parámetros
        auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        // packaged_task: encapsula la función y guarda el resultado en el future
        auto task_ptr = std::make_shared<std::packaged_task<RetType()>>(func);
        
        auto fut = task_ptr->get_future();

        {
            std::unique_lock<std::mutex> lock(m);
            if(closed)
                throw std::runtime_error("ThreadPool closed");

            // Encolo una lambda void() que ejecuta la task
            // Esto permite que todos los workers ejecuten tareas con la misma interfaz
            q.emplace([task_ptr]() {
                (*task_ptr)(); // ejecuta f(args...) y setea el future
            });
        }

        cv.notify_one();
        return fut;
    }

    void closePool(){
        {
            std::lock_guard<std::mutex> guard(m);
            if(closed)
                return;
            closed = true;
        }
        cv.notify_all();
    }

    ~ThreadPool(){
        closePool();
        for(auto& t : threads)
            t.join();
    }
};

int f1(int x, int y){
    return x + y;
}

int f2(int x, int y){
    return x * y;
}



int main(){
    ThreadPool threadPool(5);

    std::future<int> f = threadPool.addTask(f1, 1, (f2, (2, 3)));
    std::cout << f.get() << std::endl;

    threadPool.closePool();
}