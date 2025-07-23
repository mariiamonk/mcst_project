#include <iostream>
#include "../include/cache.hpp"
#include <cassert>
#include <memory>

namespace Cache{
class MemoryHierarchy {
private:
    std::shared_ptr<Cache> l1;
    std::shared_ptr<Cache> l2; 
    std::shared_ptr<MemoryModel> memory;

public:
    MemoryHierarchy(std::shared_ptr<Cache> l1,
                    std::shared_ptr<MemoryModel> mem,
                    std::shared_ptr<Cache> l2 = nullptr)
        : l1(l1), l2(l2), memory(mem){}

    OutQuery query(const InQuery& query) {
        OutQuery result;
        auto l1_result = l1->query(query);
        result.hit = l1_result.hit;
        result.returned_data = l1_result.returned_data;

        for (const auto& subquery : l1_result.out) {
            OutQuery lower_result;

            if (l2) {
                lower_result = l2->query(subquery);
            } else {
                lower_result = memory->query(subquery);
            }

            if (subquery.operation == Operation::READ && lower_result.returned_data.has_value()) {
                InQuery insert = {
                    Operation::READ,
                    subquery.address,
                    *lower_result.returned_data,
                    Data::SIZE
                };
                l1->query(insert);
                result.returned_data = lower_result.returned_data;
            }

            for (const auto& deep : lower_result.out) {
                memory->query(deep);
            }
        }
        return result;
    }
};
}
using namespace Cache;

void print_data(const Data& data) {
    for (size_t i = 0; i < data.valid_count; ++i)
        std::cout << data[i] << " ";
    std::cout << "\n";
}

int main() {
    std::cout << " Кэш 4КБ, READ_ALLOCATE, LRU\n";

    auto memory1 = std::make_shared<MemoryModel>();
    auto cache1 = std::make_shared<CacheL1>(
        4 * 1024, 64, 4, 32,
        WritePolicy::WRITE_BACK,
        AllocationPolicy::READ_ALLOCATE,
        ReplacementPolicy::LRU
    );
    MemoryHierarchy hierarchy1(cache1, memory1);

    Data test_data;
    for (size_t i = 0; i < Data::SIZE; ++i) {
        test_data[i] = i * 10;
    }
    test_data.valid_count = Data::SIZE;

    memory1->query(InQuery{ Operation::WRITE, 0x1000, test_data, Data::SIZE });

    std::cout << "\nWRITE по адресу 0x1000:\n";
    auto write1 = hierarchy1.query(InQuery{ Operation::WRITE, 0x1000, test_data, Data::SIZE });
    std::cout << (write1.hit ? "Write hit\n" : "Write miss\n");

    // Пробуем считать из того же адреса — должен быть miss и чтение из памяти
    std::cout << "\nREAD по адресу 0x1000:\n";
    auto read1 = hierarchy1.query(InQuery{ Operation::READ, 0x1000, {}, Data::SIZE });
    std::cout << (read1.hit ? "Read hit\n" : "Read miss\n");
    if (read1.get_data()) {
        std::cout << "Данные: ";
        print_data(*read1.get_data());
    } else {
        std::cout << "Данные не получены\n";  
    }

    std::cout << "\nЧтение других блоков (вызов вытеснений):\n";
    for (int i = 1; i <= 10; ++i) {
        uint64_t addr = 0x1000 + i * 64;
        auto res = hierarchy1.query(InQuery{ Operation::READ, addr, {}, Data::SIZE });
        std::cout << "READ 0x" << std::hex << addr << std::dec
                  << (res.hit ? " hit" : " miss");
        if (res.evicted)
            std::cout << " (вытеснен тег: " << res.evicted_tag << ")";
        std::cout << "\n";
    }

    std::cout << "\nСостояние кэша L1:\n";
    cache1->print_cache_state();

    std::cout << "\nИерархия памяти 2\n";

    auto memory2 = std::make_shared<MemoryModel>();

    auto l1 = std::make_shared<CacheL1>(
        16 * 1024, 32, 4, 32,
        WritePolicy::WRITE_BACK,
        AllocationPolicy::BOTH,
        ReplacementPolicy::MRU
    );

    auto l2 = std::make_shared<CacheL1>(
        256, 32, 8, 32,  // полностью ассоциативный
        WritePolicy::WRITE_THROUGH,
        AllocationPolicy::WRITE_ALLOCATE,
        ReplacementPolicy::LRU
    );

    MemoryHierarchy hierarchy2(l1, memory2, l2);

    memory2->query(InQuery{ Operation::WRITE, 0x2000, test_data, Data::SIZE });

    std::cout << "\nREAD по адресу 0x2000:\n";
    auto read2 = hierarchy2.query(InQuery{ Operation::READ, 0x2000, {}, Data::SIZE });
    std::cout << (read2.hit ? "Read hit\n" : "Read miss\n");
    if (read2.get_data()) {
        std::cout << "Данные: ";
        print_data(*read2.get_data());
    }

    std::cout << "\nWRITE по адресу 0x2000:\n";
    auto write2 = hierarchy2.query(InQuery{ Operation::WRITE, 0x2000, test_data, Data::SIZE });
    std::cout << (write2.hit ? "Write hit\n" : "Write miss\n");

    std::cout << "\nСостояние L1:\n";
    l1->print_cache_state();

    std::cout << "\nСостояние L2:\n";
    l2->print_cache_state();

    return 0;
}
