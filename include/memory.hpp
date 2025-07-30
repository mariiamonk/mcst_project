#pragma once

#include "cache.hpp"
#include <memory>
#include <sstream>

namespace Cache{
class MemoryModel {
    private:
        std::unordered_map<uint64_t, Data> memory;

    public:
        OutQuery query(const InQuery& in) {
            OutQuery result;
            result.hit = true;
            uint64_t aligned_addr = in.address & ~(Data::SIZE - 1);
            
            if (in.operation == Operation::READ) {
                if (memory.count(aligned_addr)) {
                    result.returned_data = memory[aligned_addr];
                }
            } else if (in.operation == Operation::WRITE) {
                memory[aligned_addr] = in.data; 
                memory[aligned_addr].valid_count = in.data.valid_count;  
                std::cout << "add data in memory" << std::endl;
            }
            return result;
        }

        void print_memory() {
            if (memory.empty()) {
                std::cout << "Memory is empty" << std::endl;
                return;
            }

            std::cout << "\nMemory Contents" << std::endl << "Address | Data" << std::endl;

            for (const auto& [address, data] : memory) {
                std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << address << " | ";

                if (data.valid_count == 0) {
                    std::cout << "<empty>";
                } else {
                    for (size_t i = 0; i < data.valid_count; ++i) {
                        std::cout << std::dec << data[i];
                        if (i < data.valid_count - 1) {
                            std::cout << ", ";
                        }
                    }
                }
                std::cout << std::endl;
            }
        }
    };

class MemoryHierarchy {
private:
    std::vector<std::shared_ptr<Cache>> caches;
    std::shared_ptr<MemoryModel> memory;

    void update_cache_level(size_t level, uint64_t address, const Data& data) {
        auto& cache = caches[level];
        if (cache->get_write_policy() == WritePolicy::WRITE_THROUGH) {
            InQuery update{Operation::WRITE, address, data};
            cache->query(update);
        } else {
            uint64_t tag = cache->get_tag(address);
            uint64_t index = cache->get_index(address);
            auto& line = cache->get_tag_store()[index];
            auto block_it = cache->find_block(line, tag);
            
            if (block_it != line.cache_line.end()) {
                InQuery update{Operation::WRITE, address, data};
                cache->query(update);
            }
        }
    }

    void update_all_levels(size_t highest_level, uint64_t address, const Data& data) {
        for (size_t i = 0; i <= highest_level && i < caches.size(); ++i) {
            update_cache_level(i, address, data);
        }
    }

public:
    MemoryHierarchy(std::vector<std::shared_ptr<Cache>> cache_levels,
                   std::shared_ptr<MemoryModel> mem)
        : caches(cache_levels), memory(mem) {}

    OutQuery query(const InQuery& query) {
        OutQuery final_result;
        bool hit = false;
        size_t hit_level = 0;

        for (size_t i = 0; i < caches.size(); ++i) {
            auto& cache = caches[i];
            auto cache_result = cache->query(query);
            
            if (cache_result.hit) {
                final_result = cache_result;
                hit = true;
                hit_level = i;
                break;
            }
        }

        if (!hit) {
            auto mem_result = memory->query(query);
            final_result = mem_result;

            if (query.operation == Operation::READ && mem_result.returned_data) {
                update_all_levels(caches.size() - 1, query.address, *mem_result.returned_data);
            }
        }
        //если было попадание, кеши выше обновляются 
        else if (hit_level > 0 && final_result.returned_data) {
            update_all_levels(hit_level - 1, query.address, *final_result.returned_data);
        }

        return final_result;
    }

    void add_cache_level(std::shared_ptr<Cache> cache) {
        caches.push_back(cache);
    }

    void print_caches_state(){
        for(auto cache: caches){
            cache->print_cache_state();
        }
        memory->print_memory();
    }


};

void process_commands(MemoryHierarchy& hierarchy) {
    std::string line;
    std::cout << "Enter commands (ld <size> <addr> | st <size> <addr> data) | show:" << std::endl << "> ";

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string op;
        size_t size;
        uint64_t address;
        Data data;

        iss >> op >> size >> std::hex >> address >> std::dec;

        if (op == "st") {
            std::vector<int> values;
            int val;
            while (iss >> val) {
                values.push_back(val);
            }
            
            if (values.size() > Data::SIZE) {
                std::cerr << "Error: Too much data (max " << Data::SIZE << " elements)" << std::endl;
                continue;
            }
            
            if (!values.empty()) {
                data.fill(values.data(), values.size());
            }
        }

        InQuery query{
            (op == "ld") ? Operation::READ : Operation::WRITE,
            address,
            data,
            size
        };

        OutQuery result = hierarchy.query(query);

        if (op == "ld") {
            std::cout << (result.hit ? "Hit" : "Miss") << " at 0x" << std::hex << address << std::dec << std::endl;
            if (result.hit && result.returned_data) {
                std::cout << "Data: ";
                for (size_t i = 0; i < result.returned_data->valid_count; ++i) {
                    std::cout << (*result.returned_data)[i] << " ";
                }
                std::cout << std::endl;
            }
        } else {
            std::cout << (result.hit ? "Hit" : "Miss") << " at 0x" << std::hex << address << std::dec << std::endl;
        }

        if (line == "show") {
            hierarchy.print_caches_state();
        }

        std::cout << "> ";
    }
}
}