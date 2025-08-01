#pragma once

#include "cache.hpp"
#include <memory>
#include <sstream>

namespace Cache {

class MemoryModel {
private:
    std::unordered_map<uint64_t, Data> _memory;

public:
    OutQuery query(const InQuery& in);
    void print_memory();
};

class MemoryHierarchy {
private:
    std::vector<Cache> _caches;
    std::shared_ptr<MemoryModel> _memory;

    void update_cache_level(size_t level, uint64_t address, const Data& data);
    void update_all_levels(size_t highest_level, uint64_t address, const Data& data);

public:
    MemoryHierarchy(std::vector<Cache> cache_levels,
                   std::shared_ptr<MemoryModel> mem)
        : _caches(cache_levels), _memory(mem) {}

    OutQuery query(const InQuery& query);

    void add_cache_level(Cache cache);
    void print_caches_state();
};

void process_commands(MemoryHierarchy& hierarchy);

}