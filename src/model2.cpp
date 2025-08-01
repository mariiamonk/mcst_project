#include "memory.hpp"

using namespace Cache;

int main() {
    Cache::Cache l1_cache(
        16 * 1024, 32, 4, 32,
        WritePolicy::WRITE_BACK,
        AllocationPolicy::BOTH,
        ReplacementPolicy::MRU
    );

    Cache::Cache l2_cache(
        256, 32, 256/32, 32,
        WritePolicy::WRITE_THROUGH,
        AllocationPolicy::WRITE_ALLOCATE,
        ReplacementPolicy::LRU
    );

    auto memory = std::make_shared<MemoryModel>();
    MemoryHierarchy hierarchy({l1_cache, l2_cache}, memory);

    std::cout << "L1: 16KB, 32B blocks, 4-way, BOTH-Allocate, Write-Back, MRU" << std::endl
    << "L2: 256B, 32B blocks, Fully-Assoc, Write-Allocate, Write-Through, LRU" << std::endl;
    
    process_commands(hierarchy);
    return 0;
}