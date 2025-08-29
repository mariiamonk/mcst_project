#include "../include/memory.hpp"
#include <fstream>

namespace Cache {
    void MemoryModel::initialize(MemoryInitMode mode) {
        _memory.clear();
        Data zero_data;
        zero_data.valid_count = Data::SIZE;
        std::fill(zero_data.buffer.begin(), zero_data.buffer.end(), 0);

        if (mode == MemoryInitMode::ADDRESSES) {
            for (uint64_t addr = 0; addr < 0x1000; addr += Data::SIZE * sizeof(int)) { 
                Data d;
                for (size_t i = 0; i < Data::SIZE; ++i) {
                    d[i] = static_cast<int>(addr + i * sizeof(int));
                }
                d.valid_count = Data::SIZE;
                _memory[addr] = d;
            }
        } else {
            for (uint64_t addr = 0; addr < 0x1000; addr += Data::SIZE * sizeof(int)) {
                _memory[addr] = zero_data;
            }
        }
    }

    OutQuery MemoryModel::query(const InQuery& in) {
        OutQuery result;
        result.hit = true;
        
        size_t size_bytes = in.size ; 
        size_t elements = (size_bytes + sizeof(int) - 1) / sizeof(int); 
        elements = std::min(elements, Data::SIZE); 

        uint64_t aligned_addr = in.address & ~(Data::SIZE * sizeof(int) - 1);
        
        if (_trace_level >= TraceLevel::FULL) {
            std::cout << "MEM: " << (in.operation == Operation::READ ? "READ" : "WRITE")
                    << " addr=0x" << std::hex << in.address << std::dec
                    << " size_bits=" << in.size 
                    << " (" << elements << " elements)" << std::endl;
        }

        if (in.operation == Operation::READ) {
            if (_memory.count(aligned_addr)) {
                Data& stored_data = _memory[aligned_addr];
                Data response;
                size_t elements_to_read = std::min(elements, stored_data.valid_count);
                stored_data.read_data(response.buffer.data(), elements_to_read);
                response.valid_count = elements_to_read;
                result.returned_data = response;
            }
        } else { // WRITE
            mark_modified(aligned_addr);
            
            Data& target = _memory[aligned_addr];
            size_t elements_to_write = std::min(elements, Data::SIZE);
            target.write_data(in.data.buffer.data(), elements_to_write);
            target.valid_count = elements_to_write;
            
            if (_trace_level >= TraceLevel::FULL) {
                std::cout << "MEM: WRITE data=[";
                for (size_t i = 0; i < elements_to_write; ++i) {
                    std::cout << in.data[i];
                    if (i < elements_to_write - 1) std::cout << ", ";
                }
                std::cout << "]" << std::endl;
            }
        }
        return result;
    }

    void MemoryHierarchy::log_query(size_t level, const InQuery& query, const OutQuery& result) {
        if (_trace_level == TraceLevel::NONE) return;
        
        std::cout << "L" << level << ": " 
                << (query.operation == Operation::READ ? "READ" : "WRITE")
                << " addr=0x" << std::hex << query.address << std::dec
                << " size=" << query.size
                << " - " << (result.hit ? "HIT" : "MISS");
        
        if (_trace_level >= TraceLevel::FULL) {
            if (result.returned_data) {
                std::cout << " data=[";
                for (size_t i = 0; i < result.returned_data->valid_count; ++i) {
                    std::cout << (*result.returned_data)[i];
                    if (i < result.returned_data->valid_count - 1) std::cout << ", ";
                }
                std::cout << "]";
            }
            if (result.evicted) {
                std::cout << " evicted=0x" << std::hex << result.evicted_tag << std::dec;
            }
        }
        std::cout << std::endl;
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
        
        for (size_t level = 0; level < _caches.size() && !request_completed; ++level) {
            auto& cache = _caches[level];
            OutQuery cache_result = cache->query(query);

            log_query(level, query, cache_result);
            
            if (query.operation == Operation::WRITE && 
                cache->get_write_policy() == WritePolicy::WRITE_THROUGH &&
                cache_result.hit) {
                for (auto& mem_query : cache_result.out) {
                    if (level + 1 < _caches.size()) {
                        _caches[level+1]->query(mem_query);
                    } else {
                        _memory->query(mem_query);
                    }
                }
                final_result = cache_result;
                request_completed = true;
                continue;
            }

            if (cache_result.hit) {
                final_result = cache_result;
                request_completed = true;
            } else {
                if (cache->should_allocate(query.operation)) {
                    for (auto& mem_query : cache_result.out) {
                        OutQuery next_result;
                        if (level + 1 < _caches.size()) {
                            next_result = _caches[level+1]->query(mem_query);
                        } else {
                            next_result = _memory->query(mem_query);
                        }

                        if (query.operation == Operation::READ && next_result.returned_data) {
                            uint64_t block_mask = ~(16 - 1);
                            uint64_t aligned_addr = query.address & block_mask;
                            
                            InQuery update_query{
                                Operation::WRITE,
                                aligned_addr,
                                *next_result.returned_data,
                                16
                            };
                            
                            cache->query(update_query);
                            
                            Data response_data;
                            size_t offset = query.address - aligned_addr;
                            size_t elements_to_read = std::min(
                                (query.size + sizeof(int) - 1) / sizeof(int),
                                Data::SIZE - offset / sizeof(int)
                            );
                            
                            next_result.returned_data->read_data(
                                response_data.buffer.data(),
                                elements_to_read,
                                offset / sizeof(int)
                            );
                            response_data.valid_count = elements_to_read;
                            
                            final_result.returned_data = response_data;
                            final_result.hit = true;
                            request_completed = true;
                        }
                    }
                } else {
                   
                    if (level + 1 < _caches.size()) {
                        final_result = _caches[level+1]->query(query);
                    } else {
                        final_result = _memory->query(query);
                    }
                    request_completed = true;
                }
            }
        }
        if (!request_completed) {
            final_result = _memory->query(query);
        }
        
        return final_result;
    }


    void MemoryHierarchy::print_caches_state() {
        for(auto cache: _caches) {
            cache->print_cache_state();
        }
        _memory->print_modified_memory();
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

    void run_tests(const std::string& test_file, std::shared_ptr<MemoryHierarchy> hierarchy) {
        std::ifstream infile(test_file);
        if (!infile.is_open()) {
            std::cerr << "Error: Cannot open test file: " << test_file << std::endl;
            exit(1);
        }
        std::string line;
        
        while (std::getline(infile, line)) {
            std::istringstream iss(line);
            std::string op;

            if (line.empty() || std::all_of(line.begin(), line.end(), ::isspace)) {
            continue;
        }

            std::cout <<"\n" << line << "\n";

            if (line == "show") {
                hierarchy->print_caches_state();
                continue;
            }
            size_t size;
            uint64_t address;
            Data data;

            if (!(iss >> op >> size >> std::hex >> address >> std::dec)) {
                std::cerr << "Invalid command format\n> ";
                continue;
            }

            if (op == "st") {
                std::vector<int> values;
                std::string val_str;
                while (iss >> val_str) {
                    try {
                        uint32_t val = std::stoul(val_str, nullptr, 16);
                        values.push_back(static_cast<int>(val));
                    } catch (...) {
                        std::cerr << "Invalid value format: " << val_str << "\n> ";
                        continue;
                    }
                }
                
                size_t size_bytes = size ; 
                size_t expected_values = size_bytes / sizeof(int);
                
                if (values.size() != expected_values) {
                    std::cerr << "Size mismatch. Expected " << expected_values
                            << " values, got " << values.size() << " values\n> ";
                    continue;
                }

                Data new_data;
                std::fill(new_data.buffer.begin(), new_data.buffer.end(), 0); 
                std::copy(values.begin(), values.end(), new_data.buffer.begin()); 
                
                data = new_data;
            }

            InQuery query{
                (op == "ld") ? Operation::READ : Operation::WRITE,
                address,
                data,
                size
            };

            OutQuery result = hierarchy->query(query);
            
            if (op == "ld" && result.hit && result.returned_data) {
                std::cout << "Data: ";
                const auto& resp_data = *result.returned_data;
                size_t elements_to_print = std::min(
                    (query.size + 31) / 32, 
                    resp_data.valid_count
                );
                for (size_t i = 0; i < elements_to_print; ++i) {
                    std::cout << resp_data.buffer[i] << " ";
                }
                std::cout << std::endl;
            }
            hierarchy->print_changes();
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
        std::cout << "Enter commands (ld <size> <addr> | st <size> <addr> <val1> <val2> ...) | show:" << std::endl;

        while (std::getline(std::cin, line)) {
            std::istringstream iss(line);
            std::string op;

            if(line == "/n"){continue;}
            if (line == "show") {
                hierarchy->print_caches_state();
                continue;
            }
            size_t size;
            uint64_t address;
            Data data;

            if (!(iss >> op >> size >> std::hex >> address >> std::dec) and line != "/n") {
                std::cerr << "Invalid command format\n> ";
                continue;
            }


            if (op == "st") {
                std::vector<int> values;
                std::string val_str;
                while (iss >> val_str) {
                    try {
                        uint32_t val = std::stoul(val_str, nullptr, 16);
                        values.push_back(static_cast<int>(val));
                    } catch (...) {
                        std::cerr << "Invalid value format: " << val_str << "\n> ";
                        continue;
                    }
                }

                size_t expected_values = size / sizeof(int);
                
                if (values.size() != expected_values) {
                    std::cerr << "Size mismatch. Expected " << expected_values
                            << " values, got " << values.size() << " values\n> ";
                    continue;
                }

                Data new_data;
                for (size_t i = 0; i < values.size(); ++i) {
                    new_data[i] = values[i];
                }
                new_data.valid_count = Data::SIZE; 
                
                data = new_data;
            }

            InQuery query{
                (op == "ld") ? Operation::READ : Operation::WRITE,
                address,
                data,
                size
            };

            OutQuery result = hierarchy->query(query);
            
            if (op == "ld" && result.hit && result.returned_data) {
                std::cout << "Data: ";
                const auto& resp_data = *result.returned_data;
                for (size_t i = 0; i < resp_data.valid_count; ++i) {
                    std::cout << resp_data.buffer[i] << " ";
                }
                std::cout << std::endl;
            }
            hierarchy->print_changes();
        }
    }

    boost::program_options::options_description create_options_description()
    {
        boost::program_options::options_description desc("Single Cache Model Options");
        desc.add_options()
            ("help,h", "Show help message")
            ("trace,t", boost::program_options::value<int>()->default_value(0), 
            "Trace level (0=none, 1=basic, 2=full)")
            ("init,i", boost::program_options::value<int>()->default_value(0), 
            "Memory init mode (0=zeros, 1=addresses)")
            ("test", boost::program_options::value<std::string>(), 
            "Run test from file");
        return desc;
    }

    boost::program_options::variables_map parse_command_line_args(
        int argc, char* argv[], 
        const boost::program_options::options_description& desc)
    {
        boost::program_options::variables_map vm;
        boost::program_options::store(
            boost::program_options::parse_command_line(argc, argv, desc), vm);
        boost::program_options::notify(vm);
        return vm;
    }

    bool handle_help_option(const boost::program_options::variables_map& vm,
                                const boost::program_options::options_description& desc)
    {
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return true;
        }
        return false;
    }

    TraceLevel get_trace_level(const boost::program_options::variables_map& vm)
    {
        return static_cast<TraceLevel>(vm["trace"].as<int>());
    }

    MemoryInitMode get_memory_init_mode(const boost::program_options::variables_map& vm)
    {
        return static_cast<MemoryInitMode>(vm["init"].as<int>());
    }
}