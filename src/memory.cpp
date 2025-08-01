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
            std::cout << "add data in memory" << std::endl;
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
        return final_result;
    }

    void MemoryHierarchy::add_cache_level(Cache cache) {
        _caches.push_back(cache);
    }

    void MemoryHierarchy::print_caches_state() {
        for(auto cache: _caches) {
            cache.print_cache_state();
        }
        _memory->print_memory();
    }

    void MemoryHierarchy::update_cache_level(size_t level, uint64_t address, const Data& data) {
        auto& cache = _caches[level];
        if (cache.get_write_policy() == WritePolicy::WRITE_THROUGH) {
            InQuery update{Operation::WRITE, address, data};
            cache.query(update);
        } else {
            uint64_t tag = cache.get_tag(address);
            uint64_t index = cache.get_index(address);
            auto& line = cache.get_tag_store()[index];
            auto block_it = cache.find_block(line, tag);
            
            if (block_it != line.cache_line.end()) {
                InQuery update{Operation::WRITE, address, data};
                cache.query(update);
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
        std::cout << (hit ? "Hit" : "Miss") << " at 0x" 
                  << std::hex << address << std::dec << std::endl;
        
        if (op == "ld" && hit && returned_data) {
            std::cout << "Data: ";
            for (size_t i = 0; i < returned_data->valid_count; ++i) {
                std::cout << (*returned_data)[i] << " ";
            }
            std::cout << std::endl;
        }
    };

    void process_commands(MemoryHierarchy& hierarchy) {
        std::string line;
        std::cout << "Enter commands (ld <size> <addr> | st <size> <addr> data) | show:" << std::endl;

        while (std::getline(std::cin, line)) {
            std::cout << "> ";

            if (line == "show") {
                hierarchy.print_caches_state();
                continue;
            }

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
                
                if (values.size() > Data::SIZE) {
                    std::cerr << "Error: Too much data (max " << Data::SIZE << " elements)\n> ";
                    continue;
                }
                
                if (!values.empty()) {
                    data.fill(values.data(), values.size());
                }
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

            OutQuery result = hierarchy.query(query);
            print_result(op, result.hit, address, result.returned_data);
        }
    }

}