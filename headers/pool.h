#ifndef THREAD_POOL
#define THREAD_POOL

#include "blockingQueue.h"
#include <functional>
#include <future>
#include <type_traits>

class ThreadPool{
private:
    BlockingQueue<std::function<void()>> q;
    std::vector<std::thread> threads;
    
private:
    void worker(){
        std::function<void()> f;
        while(q.pop(f)){
            f();
        }
    }
    
public:
    ThreadPool(const size_t consumer_count, const size_t max_size_queue = 10) : q(max_size_queue){
        for(size_t i{}; i < consumer_count; i++){
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
            Uso una lambda, que me permite mantener perfect Forwarding a su vez que encapsulo la funcion
        */
        auto func_wrapper = 
            [funct = std::forward<F>(f),
            tup = std::make_tuple(std::forward<Args>(args)...)]() mutable{
                /*
                    Apply es un invoke para tuplas
                    paso la tupla con move, para que pueda mover los valores almacenados
                */
                return std::apply(funct, std::move(tup));
        };

        // Uso package_task para almacenar mi funcion (poder usarla mas tarde) y su resultado en un future.
        // Uso make_shared para que mi funcion almacenada en el packaged no muera al finalizar mi funcion 
        // (la lambda que encolo tiene una referencia a este packaged)
        auto task_ptr = std::make_shared<std::packaged_task<RetType()>>(func_wrapper);
        // ACLARO, cuando tengo el RetType(), los parentesis vacios significa que no toma ningun parametro
        
        auto fut = task_ptr->get_future();

        // Encolo una lambda void() que ejecuta la task
        // Esto permite que todos los workers ejecuten tareas con la misma interfaz
        if(!q.push([task_ptr](){ (*task_ptr)(); /* ejecuta f(args...) y setea el future */}))
            throw std::runtime_error("ThreadPool closed");

        return fut;
    }

    template<typename F, typename... Args>
    void addTaskDetached(F&& f, Args&&... args){

        auto funct_wrapper = 
            [funct = std::forward<F>(f),
            tup = std::make_tuple(std::forward<Args>(args)...)]() mutable{
                std::apply(funct, std::move(tup));
        };

        /*
            Uso una lambda, y no le paso directamtne el move de funct_wrapper:
            Mas adelante podria querer meter mas logica antro de esa funcion, como metricas, etc
        */
        if(!q.push([func = std::move(funct_wrapper)]() mutable {func();}))
            throw std::runtime_error("ThreadPool closed");
    }

    void closePool(){
        q.close();
    }

    ~ThreadPool(){
        closePool();
        for(auto& t : threads)
            t.join();
    }
};

#endif