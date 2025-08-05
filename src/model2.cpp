#include "memory.hpp"

using namespace Cache;

int main() {
    auto l1_cache = std::make_shared<Cache::Cache>(
        16 * 1024, 32, 4, 32,
        WritePolicy::WRITE_BACK,
        AllocationPolicy::BOTH,
        ReplacementPolicy::MRU
    );

    auto l2_cache = std::make_shared<Cache::Cache>(
        256, 32, 256/32, 32,
        WritePolicy::WRITE_THROUGH,
        AllocationPolicy::WRITE_ALLOCATE,
        ReplacementPolicy::LRU
    );

    auto memory = std::make_shared<MemoryModel>();
    std::vector<std::shared_ptr<Cache::Cache>> caches;
    caches.push_back(l1_cache);
    caches.push_back(l2_cache);
    auto hierarchy = std::make_shared<MemoryHierarchy>(caches, memory);

    std::cout << "L1: 16KB, 32B blocks, 4-way, BOTH-Allocate, Write-Back, MRU" << std::endl
    << "L2: 256B, 32B blocks, Fully-Assoc, Write-Allocate, Write-Through, LRU" << std::endl;
    
    process_commands(hierarchy);
    return 0;
}