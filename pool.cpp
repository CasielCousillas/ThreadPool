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

        // Uso un lambda, ya que copia los argumentos y me permite encapsularlos
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

std::string f1(std::string word){
    return word;
}

int f2(int x, int y){
    return x * y;
}

int f3(int& x){
    return x = 10;
}

// Tratar que funcione con sobrecarga de funciones
int main(){
    ThreadPool threadPool(5);
    std::string a = "hola";
    std::string b = "adios";
    int x = 2;

    std::future<int> f = threadPool.addTask(f3, std::ref(x));
    std::cout << f.get() << std::endl;

    threadPool.closePool();

    std::cout << x << std::endl;
}