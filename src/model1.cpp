#include "memory.hpp"

using namespace Cache;

int main() {
    auto cache = Cache::Cache(
        4 * 1024, 64, 4, 32,
        WritePolicy::WRITE_BACK,
        AllocationPolicy::READ_ALLOCATE,
        ReplacementPolicy::LRU
    );
    
    auto memory = std::make_shared<MemoryModel>();
    MemoryHierarchy hierarchy({cache}, memory);

    std::cout << "Cache: 4KB, 64B blocks, 4-way, Read-Allocate, Write-Back, LRU\n";
    
    process_commands(hierarchy);
    return 0;
}