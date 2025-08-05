#include "../include/memory.hpp"

using namespace Cache;

// void print_cache_operation(const InQuery& query, const OutQuery& result, bool evicted) {
//     std::cout << "Operation: " << (query.operation == Operation::READ ? "READ" : "WRITE")
//               << ", Address: 0x" << std::hex << query.address << std::dec
//               << ", Hit: " << (result.hit ? "YES" : "NO");
    
//     if (evicted) {
//         std::cout << ", Evicted block with tag: 0x" << std::hex << result.evicted_tag << std::dec;
//     }
//     std::cout << std::endl;
// }

// void test_cache_operations() {
//     auto l1_cache = std::make_shared<Cache::Cache>(32, 16, 2, 32, 
//                                       WritePolicy::WRITE_BACK, 
//                                       AllocationPolicy::READ_ALLOCATE, 
//                                       ReplacementPolicy::LRU);
    
//     auto memory = std::make_shared<MemoryModel>();

//     l1_cache->print_cache_state();

//     auto create_data = [](std::initializer_list<int> values) {
//         Data d;
//         int i = 0;
//         for (auto val : values) {
//             d[i++] = val;
//         }
//         d.valid_count = values.size();
//         return d;
//     };

//     Data data1 = create_data({1, 2, 3, 4});
//     Data data2 = create_data({5, 6, 7, 8});
//     Data data3 = create_data({9, 10, 11, 12});
//     Data data4 = create_data({13, 14, 15, 16});

//     auto process_query = [&](const InQuery& query) -> OutQuery {
//         OutQuery result;
        
//         auto cache_result = l1_cache->query(query);
//         result = cache_result;
        
//         if (!cache_result.hit && cache_result.needs_memory_access()) {
//             for (const auto& mem_query : cache_result.out) {
//                 auto mem_result = memory->query(mem_query);
                
//                 if (mem_query.operation == Operation::READ && mem_result.returned_data) {
//                     InQuery update_cache{
//                         Operation::WRITE,
//                         mem_query.address,
//                         *mem_result.returned_data,
//                         Data::SIZE
//                     };
//                     update_cache.data.valid_count = mem_result.returned_data->valid_count;
//                     l1_cache->query(update_cache);
//                     result.returned_data = mem_result.returned_data;
//                 }
//             }
//         }
        
//         return result;
//     };

//         std::cout << "\nWrite to address 0x10" << std::endl;
//         InQuery query{Operation::WRITE, 0x10, data1};
//         OutQuery result = process_query(query);
//         print_cache_operation(query, result, result.evicted);
//         l1_cache->print_cache_state();
//         memory->print_memory();

//         {
//         std::cout << "\nWrite to address 0x20" << std::endl;
//         InQuery query{Operation::WRITE, 0x20, data2};
//         OutQuery result = process_query(query);
//         print_cache_operation(query, result, result.evicted);
//         l1_cache->print_cache_state();
//         memory->print_memory();
//         }
//         {
//         std::cout << "\nRead from address 0x10" << std::endl;
//         InQuery query{Operation::READ, 0x10, Data{}, Data::SIZE};
//         OutQuery result = process_query(query);
//         print_cache_operation(query, result, result.evicted);
//         }
//         if (result.hit && result.returned_data) {
//             std::cout << "Data: ";
//             for (size_t i = 0; i < result.returned_data->valid_count; ++i) {
//                 std::cout << (*result.returned_data)[i] << " ";
//             }
//             std::cout << std::endl;
//         }
//         l1_cache->print_cache_state();
//         memory->print_memory();

//         {
//         std::cout << "\nWrite to address 0x30" << std::endl;
//         InQuery query{Operation::WRITE, 0x30, data3};
//         OutQuery result = process_query(query);
//         print_cache_operation(query, result, result.evicted);
//         l1_cache->print_cache_state();
//         memory->print_memory();
//         }
//         {
//         std::cout << "\nTry to read evicted block" << std::endl;
//         InQuery query{Operation::READ, 0x20, Data{}, Data::SIZE};
//         OutQuery result = process_query(query);
//         print_cache_operation(query, result, result.evicted);
//         l1_cache->print_cache_state();
//         memory->print_memory();
//         }
//         {   
//         std::cout << "\nWrite to address 0x40" << std::endl;
//         InQuery query{Operation::WRITE, 0x40, data4};
//         OutQuery result = process_query(query);
//         print_cache_operation(query, result, result.evicted);
//         l1_cache->print_cache_state();
//         memory->print_memory();
//         }
// }

// int main() {
//     test_cache_operations();
//     return 0;
// }



void print_operation_result(const InQuery& query, const OutQuery& result) {
    std::cout << "\nOperation: " << (query.operation == Operation::READ ? "READ" : "WRITE")
              << ", Address: 0x" << std::hex << query.address << std::dec
              << ", Hit: " << (result.hit ? "YES" : "NO");
    
    if (result.evicted) {
        std::cout << ", Evicted tag: 0x" << std::hex << result.evicted_tag << std::dec;
    }
    
    if (result.returned_data) {
        std::cout << ", Data: ";
        for (size_t i = 0; i < result.returned_data->valid_count; ++i) {
            std::cout << (*result.returned_data)[i] << " ";
        }
    }
    std::cout << std::endl;
}

Data create_data(std::initializer_list<int> values) {
    Data d;
    int i = 0;
    for (auto val : values) {
        d[i++] = val;
    }
    d.valid_count = values.size();
    return d;
}

void test_hierarchy(std::shared_ptr<MemoryHierarchy> hierarchy) {
    Data data1 = create_data({1, 2, 3, 4});
    Data data2 = create_data({5, 6, 7, 8});
    Data data3 = create_data({9, 10, 11, 12});
    Data data4 = create_data({13, 14, 15, 16});

    std::cout << "\nwrite";
    InQuery write1{Operation::WRITE, 0x10, data1};
    OutQuery result1 = hierarchy->query(write1);
    print_operation_result(write1, result1);
    hierarchy->print_caches_state();

    std::cout << "\nread";
    InQuery read1{Operation::READ, 0x10, Data{}, Data::SIZE};
    OutQuery result2 = hierarchy->query(read1);
    print_operation_result(read1, result2);
    hierarchy->print_caches_state();

    std::cout << "\nwrite to new address";
    InQuery write2{Operation::WRITE, 0x20, data2};
    OutQuery result3 = hierarchy->query(write2);
    print_operation_result(write2, result3);
    hierarchy->print_caches_state();

    std::cout << "\n";
    OutQuery result4 = hierarchy->query(read1);
    print_operation_result(read1, result4);
    hierarchy->print_caches_state();

    std::cout << "\n";
    InQuery write3{Operation::WRITE, 0x30, data3};
    OutQuery result5 = hierarchy->query(write3);
    print_operation_result(write3, result5);
    hierarchy->print_caches_state();

    std::cout << "\n";
    InQuery read2{Operation::READ, 0x40, Data{}, Data::SIZE};
    OutQuery result6 = hierarchy->query(read2);
    print_operation_result(read2, result6);
    hierarchy->print_caches_state();

    std::cout << "\n";
    InQuery write4{Operation::WRITE, 0x50, data4};
    OutQuery result7 = hierarchy->query(write4);
    print_operation_result(write4, result7);
    hierarchy->print_caches_state();
}

int main() {
    auto memory = std::make_shared<MemoryModel>();

    {
        std::cout << "\n\n2xWRITE-BACK,READ-ALLOCATE,LRU\n";
        auto l1_cache = std::make_shared<Cache::Cache>(32, 16, 2, 32, 
                                      WritePolicy::WRITE_BACK, 
                                      AllocationPolicy::READ_ALLOCATE, 
                                      ReplacementPolicy::LRU);
        
        auto l2_cache = std::make_shared<Cache::Cache>(64, 16, 4, 32,
                                      WritePolicy::WRITE_BACK,
                                      AllocationPolicy::READ_ALLOCATE,
                                      ReplacementPolicy::LRU);

        std::vector<std::shared_ptr<Cache::Cache>> caches;
        caches.push_back(l1_cache);
        caches.push_back(l2_cache);
        
        auto hierarchy = std::make_shared<MemoryHierarchy>(caches, memory);
        
        test_hierarchy(hierarchy);
    }

    {
        std::cout << "\n\n2xWRITE_THROUGH,WRITE_ALLOCATE,LRU\n";
        auto l1_cache = std::make_shared<Cache::Cache>(32, 16, 2, 32, 
                                      WritePolicy::WRITE_THROUGH, 
                                      AllocationPolicy::WRITE_ALLOCATE, 
                                      ReplacementPolicy::LRU);
        
        auto l2_cache = std::make_shared<Cache::Cache>(64, 16, 4, 32,
                                      WritePolicy::WRITE_THROUGH,
                                      AllocationPolicy::WRITE_ALLOCATE,
                                      ReplacementPolicy::LRU);

        std::vector<std::shared_ptr<Cache::Cache>> caches;
        caches.push_back(l1_cache);
        caches.push_back(l2_cache);
        
        auto hierarchy = std::make_shared<MemoryHierarchy>(caches, memory);
        
        test_hierarchy(hierarchy);
    }

    {
        std::cout << "\n\nwRITE_BACK,READ_ALLOCATE,LRU + WRITE_THROUGH,BOTH,MRU\n";
        auto l1_cache = std::make_shared<Cache::Cache>(32, 16, 2, 32, 
                                      WritePolicy::WRITE_BACK, 
                                      AllocationPolicy::READ_ALLOCATE, 
                                      ReplacementPolicy::LRU);
        
        auto l2_cache = std::make_shared<Cache::Cache>(64, 16, 4, 32,
                                      WritePolicy::WRITE_THROUGH,
                                      AllocationPolicy::BOTH,
                                      ReplacementPolicy::MRU);

        std::vector<std::shared_ptr<Cache::Cache>> caches;
        caches.push_back(l1_cache);
        caches.push_back(l2_cache);
        
        auto hierarchy = std::make_shared<MemoryHierarchy>(caches, memory);
        
        test_hierarchy(hierarchy);
    }

    return 0;
}