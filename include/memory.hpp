#pragma once

#include "cache.hpp"
#include <memory>
#include <sstream>
#include <unordered_set>

namespace Cache {

    enum class TraceLevel {
        NONE,
        BASIC,
        FULL
    };

    enum class MemoryInitMode {
        ZEROS,
        ADDRESSES
    };

    class MemoryModel {
    private:
        std::unordered_map<uint64_t, Data> _memory;
        TraceLevel _trace_level;

        std::unordered_set<uint64_t> _modified_addresses; // Dля отслеживания измененных адресов

    public:
        MemoryModel() : _trace_level(TraceLevel::NONE) {}  
        MemoryModel(TraceLevel trace) : _trace_level(trace) {}  
        OutQuery query(const InQuery& in);
        void initialize(MemoryInitMode mode);
        void print_memory();
        void set_trace_level(TraceLevel level);

        void mark_modified(uint64_t address) {
            _modified_addresses.insert(address);
        }

        void print_modified_memory() {
            if (_modified_addresses.empty()) {
                std::cout << "No memory modifications since last print\n";
                return;
            }

            std::cout << "\nModified Memory Contents\n"  
                    << "Address | Data" << std::endl;

            for (const auto& address : _modified_addresses) {
                if (_memory.count(address)) {
                    const auto& data = _memory[address];
                    std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << address << " | ";

                    if (data.valid_count == 0) {
                        std::cout << "<empty>";
                    } else {
                        for (size_t i = 0; i < data.valid_count; ++i) {
                            std::cout << std::dec << data[i];
                            if (i < data.valid_count - 1) std::cout << ", ";
                        }
                    }
                    std::cout << std::endl;
                }
            }
        }
    };

    class MemoryHierarchy {
    private:
        std::vector<std::shared_ptr<Cache>> _caches;
        std::shared_ptr<MemoryModel> _memory;

        TraceLevel _trace_level;
        void log_query(size_t level, const InQuery& query, const OutQuery& result);

        void update_cache_level(size_t level, uint64_t address, const Data& data);
        void update_all_levels(size_t highest_level, uint64_t address, const Data& data);

    public:
        MemoryHierarchy(std::vector<std::shared_ptr<Cache>> cache_levels,
                    std::shared_ptr<MemoryModel> mem,
                    TraceLevel trace = TraceLevel::NONE)
            : _caches(std::move(cache_levels)), _memory(std::move(mem)), _trace_level(trace) {}

        OutQuery query(const InQuery& query);

        void add_cache_level(Cache cache);
        void print_caches_state();

        void print_changes() {
            _memory->print_modified_memory();
        }

    private:

    };

void process_commands(std::shared_ptr<MemoryHierarchy> hierarchy);
void run_tests(const std::string& test_file, std::shared_ptr<MemoryHierarchy> hierarchy);

}