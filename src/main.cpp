#include <iostream>
#include "../include/cache.hpp"

int main() {
    using namespace Cache;
    
    // Создаем кэш с параметрами:
    // 128KB, блоки 64B, 32-битные адреса, 4-ways
    // Write-Back, Write-Allocate, LRU
    CacheL1 cache(128*1024, 64, 32, 4, 
                 WritePolicy::WRITE_BACK,
                 AllocationPolicy::WRITE_ALLOCATE,
                 ReplacementPolicy::LRU);
    
    // Тестовые данные
    Data data;
    for (int i = 0; i < 16; ++i) {
        data[i] = i + 1;
    }
    data.valid_count = 16;
    
    // Тестовый запрос
    InQuery query;
    query.operation = Operation::WRITE;
    query.address = 0x12345678;
    query.data = data;
    
    auto result = cache.query(query);
    
    // Выводим состояние
    cache.print_cache_state();
    
    if (result.hit) {
        std::cout << "Cache hit!\n";
    } else {
        std::cout << "Cache miss! ";
        if (result.evicted) {
            std::cout << "Evicted tag: 0x" 
                      << std::hex << result.evicted_tag << std::dec;
        }
        std::cout << "\n";
    }
    
    return 0;
}

//разработать модель памяти и два теста 