#include <iostream>
#include "include/cache.hpp"

int main(){
    using namespace Cache;
    
    CacheL1 l1_cache(128, 64, 4, 32);
    
    std::vector<int> test_data(16);
    for (int i = 0; i < 16; ++i) test_data[i] = i + 1;
    
    Data write_data;
    std::copy_n(test_data.data(), 16, write_data.data);
    write_data.count = 16;
    
    Data read_data;

    return 0;
}

//разработать модель памяти и два теста 