#include "../include/memory.hpp"

namespace Cache {

    OutQuery MemoryModel::query(const InQuery& in) {
        OutQuery result;
        result.hit = true;
        uint64_t aligned_addr = in.address & ~(Data::SIZE - 1);
        
        if (in.operation == Operation::READ) {
            if (_memory.count(aligned_addr)) {
                result.returned_data = _memory[aligned_addr];
            }
        } else if (in.operation == Operation::WRITE) {
            _memory[aligned_addr] = in.data; 
            _memory[aligned_addr].valid_count = in.data.valid_count;  
            std::cout << "Data written to memory at 0x" << std::hex << in.address << std::dec << std::endl;
        }
        return result;
    }

    void MemoryModel::print_memory() {
        if (_memory.empty()) {
            std::cout << "Memory is empty" << std::endl;
            return;
        }

        std::cout << "\nMemory Contents" << std::endl << "Address | Data" << std::endl;

        for (const auto& [address, data] : _memory) {
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

    
    OutQuery MemoryHierarchy::query(const InQuery& query) {
        OutQuery final_result;
        bool request_completed = false;
        InQuery current_query = query;
        
        for (size_t level = 0; level < _caches.size() && !request_completed; ++level) {
            auto& cache = _caches[level];
            OutQuery cache_result = cache->query(current_query);
            
            if (query.operation == Operation::WRITE && 
                cache->get_write_policy() == WritePolicy::WRITE_THROUGH) {
                if (!cache_result.out.empty()) {
                    auto& mem_query = cache_result.out.front();
                    if (level + 1 < _caches.size()) {
                        _caches[level+1]->query(mem_query);
                    } else {
                        _memory->query(mem_query);
                    }
                }
                
                if (cache_result.hit) {
                    final_result = cache_result;
                    request_completed = true;
                    continue;
                }
            }

            if (cache_result.hit) {
                final_result = cache_result;
                request_completed = true;
            } else {
                if (cache->should_allocate(current_query.operation)) {
                    if (cache->get_alloc_policy() == AllocationPolicy::BOTH && 
                        cache->get_write_policy() == WritePolicy::WRITE_BACK &&
                        query.operation == Operation::WRITE) {
                        
                        uint64_t aligned_addr = current_query.address & ~(Data::SIZE - 1);
                        InQuery update_query{
                            Operation::WRITE,
                            aligned_addr,
                            current_query.data,
                            Data::SIZE
                        };
                        final_result = cache->query(update_query);
                        request_completed = true;
                    } 
                    else {
                        for (auto& mem_query : cache_result.out) {
                            OutQuery next_result;
                            if (level + 1 < _caches.size()) {
                                next_result = _caches[level+1]->query(mem_query);
                            } else {
                                next_result = _memory->query(mem_query);
                            }

                            if (next_result.returned_data) {
                                uint64_t aligned_addr = mem_query.address & ~(Data::SIZE - 1);
                                InQuery update_query{
                                    Operation::WRITE,
                                    aligned_addr,
                                    *next_result.returned_data,
                                    Data::SIZE
                                };
                                auto update_result = cache->query(update_query);
                                
                                if (current_query.operation == Operation::READ) {
                                    final_result.returned_data = next_result.returned_data;
                                    final_result.hit = true;
                                    request_completed = true;
                                }
                            }
                        }
                    }
                } else {
                    if (level + 1 < _caches.size()) {
                        final_result = _caches[level+1]->query(current_query);
                    } else {
                        final_result = _memory->query(current_query);
                    }
                    request_completed = true;
                }
            }
        }
        
        if (!request_completed) {
            final_result = _memory->query(current_query);
        }
        
        return final_result;
    }

    void MemoryHierarchy::print_caches_state() {
        for(auto cache: _caches) {
            cache->print_cache_state();
        }
        _memory->print_memory();
    }

    void MemoryHierarchy::update_cache_level(size_t level, uint64_t address, const Data& data) {
        auto& cache = _caches[level];
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

    void MemoryHierarchy::update_all_levels(size_t highest_level, uint64_t address, const Data& data) {
        for (size_t i = 0; i <= highest_level && i < _caches.size(); ++i) {
            update_cache_level(i, address, data);
        }
    }

    auto print_result = [](const std::string& op, bool hit, uint64_t address, 
                          const std::optional<Data>& returned_data) {
        // std::cout << (hit ? "Hit" : "Miss") << " at 0x" 
        //           << std::hex << address << std::dec << std::endl;
        
        if (op == "ld" && hit && returned_data) {
            std::cout << "Data: ";
            for (size_t i = 0; i < returned_data->valid_count; ++i) {
                std::cout << (*returned_data)[i] << " ";
            }
            std::cout << std::endl;
        }
    };

    void process_commands(std::shared_ptr<MemoryHierarchy> hierarchy) {
        std::string line;
        std::cout << "Enter commands (ld <size> <addr> | st <size> <addr> data) | show:" << std::endl;

        while (std::getline(std::cin, line)) {
            std::cout << "> ";
            std::istringstream iss(line);

            std::string op;
            size_t size;
            uint64_t address;
            Data data;

            if (!(iss >> op >> size >> std::hex >> address >> std::dec)) {
                std::cerr << "Invalid command format\n> ";
                continue;
            }

            if (op == "st") {
                std::vector<int> values;
                int val;
                while (iss >> val) values.push_back(val);
                
                if (values.size() != size) {
                    std::cerr << "Expected " << size << " values, got " 
                            << values.size() << "\n> ";
                    continue;
                }
                
                data.fill(values.data(), size);
            } else if (op != "ld") {
                std::cerr << "Unknown operation: " << op << "\n> ";
                continue;
            }

            InQuery query{
                (op == "ld") ? Operation::READ : Operation::WRITE,
                address,
                data,
                size
            };

            OutQuery result = hierarchy->query(query);
            hierarchy->print_caches_state();
            print_result(op, result.hit, address, result.returned_data);
        }
    }
}