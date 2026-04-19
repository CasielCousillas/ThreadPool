#include <iostream>
#include "blockingQueue.h"
#include "pool.h"

std::string f1(std::string word){
    return word;
}

int f2(int x, int y){
    return x * y;
}

void f3(int x){
    std::cout << x << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

const int f4(int x){
    return x;
}

void print(const std::string& word){
    std::cout << word << std::endl;
}

// Tratar que funcione con sobrecarga de funciones
// Probar que pasa si le meto funcoines que no van o no le meto una funcion
int main(){
    ThreadPool threadPool(3, 5);

    for(int i = 0; i < 100; i++){
        threadPool.addTaskDetached(f3, i);
    }

    threadPool.closePool();
}