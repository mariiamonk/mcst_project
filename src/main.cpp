#include "../include/memory.hpp"

using namespace Cache;

void print_cache_operation(const InQuery& query, const OutQuery& result, bool evicted) {
    std::cout << "Operation: " << (query.operation == Operation::READ ? "READ" : "WRITE")
              << ", Address: 0x" << std::hex << query.address << std::dec
              << ", Hit: " << (result.hit ? "YES" : "NO");
    
    if (evicted) {
        std::cout << ", Evicted block with tag: 0x" << std::hex << result.evicted_tag << std::dec;
    }
    std::cout << std::endl;
}

void test_cache_operations() {
    auto l1_cache = std::make_shared<Cache::Cache>(32, 16, 2, 32, 
                                      WritePolicy::WRITE_BACK, 
                                      AllocationPolicy::WRITE_ALLOCATE, 
                                      ReplacementPolicy::LRU);
    
    auto memory = std::make_shared<MemoryModel>();

    l1_cache->print_cache_state();

    auto create_data = [](std::initializer_list<int> values) {
        Data d;
        int i = 0;
        for (auto val : values) {
            d[i++] = val;
        }
        d.valid_count = values.size();
        return d;
    };

    Data data1 = create_data({1, 2, 3, 4});
    Data data2 = create_data({5, 6, 7, 8});
    Data data3 = create_data({9, 10, 11, 12});
    Data data4 = create_data({13, 14, 15, 16});

    auto process_query = [&](const InQuery& query) -> OutQuery {
        OutQuery result;
        
        auto cache_result = l1_cache->query(query);
        result = cache_result;
        
        if (!cache_result.hit && cache_result.needs_memory_access()) {
            for (const auto& mem_query : cache_result.out) {
                auto mem_result = memory->query(mem_query);
                
                if (mem_query.operation == Operation::READ && mem_result.returned_data) {
                    InQuery update_cache{
                        Operation::READ,
                        mem_query.address,
                        *mem_result.returned_data,
                        Data::SIZE
                    };
                    l1_cache->query(update_cache);
                    result.returned_data = mem_result.returned_data;
                }
            }
        }
        
        return result;
    };

        std::cout << "\nWrite to address 0x10" << std::endl;
        InQuery query{Operation::WRITE, 0x10, data1};
        OutQuery result = process_query(query);
        print_cache_operation(query, result, result.evicted);
        l1_cache->print_cache_state();

        {
        std::cout << "\nWrite to address 0x20" << std::endl;
        InQuery query{Operation::WRITE, 0x20, data2};
        OutQuery result = process_query(query);
        print_cache_operation(query, result, result.evicted);
        l1_cache->print_cache_state();
        }
        {
        std::cout << "\nRead from address 0x10" << std::endl;
        InQuery query{Operation::READ, 0x10, Data{}, Data::SIZE};
        OutQuery result = process_query(query);
        print_cache_operation(query, result, result.evicted);
        }
        if (result.hit && result.returned_data) {
            std::cout << "Data: ";
            for (size_t i = 0; i < result.returned_data->valid_count; ++i) {
                std::cout << (*result.returned_data)[i] << " ";
            }
            std::cout << std::endl;
        }
        l1_cache->print_cache_state();

        {
        std::cout << "\nWrite to address 0x30" << std::endl;
        InQuery query{Operation::WRITE, 0x30, data3};
        OutQuery result = process_query(query);
        print_cache_operation(query, result, result.evicted);
        l1_cache->print_cache_state();
        }
        {
        std::cout << "\nTry to read evicted block" << std::endl;
        InQuery query{Operation::READ, 0x20, Data{}, Data::SIZE};
        OutQuery result = process_query(query);
        print_cache_operation(query, result, result.evicted);
        l1_cache->print_cache_state();
        }
        {   
        std::cout << "\nWrite to address 0x40" << std::endl;
        InQuery query{Operation::WRITE, 0x40, data4};
        OutQuery result = process_query(query);
        print_cache_operation(query, result, result.evicted);
        l1_cache->print_cache_state();
        }
}

int main() {
    test_cache_operations();
    return 0;
}