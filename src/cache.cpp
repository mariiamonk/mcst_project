#include "../include/cache.hpp"
#include <sstream>
#include <iomanip>

namespace Cache{
    auto CacheL1::query(InQuery const& query) -> OutQuery {
        OutQuery result;
        uint64_t tag = get_tag(query.address);
        uint64_t index = get_index(query.address);
        auto& line = tag_store[index];

        // Поиск блока в кэше
        auto block_it = find_block(line, tag);

        if (block_it != line.cache_line.end()) { // Cache hit
            result.hit = true;
            
            // Обновляем порядок в соответствии с политикой вытеснения
            if (repl_policy_ != ReplacementPolicy::RANDOM) {
                move_beg_block(line, block_it);
            }

            if (query.operation == Operation::READ) {
                // Возвращаем данные через out[0]
                result.out.emplace_back(InQuery{
                    Operation::READ,
                    query.address,
                    block_it->data
                });
            } else { // WRITE
                block_it->data.fill(query.data.buffer.data(), query.data.valid_count);
                block_it->dirty = true;
                
                if (write_policy_ == WritePolicy::WRITE_THROUGH) {
                    // Сквозная запись - сразу в память
                    result.out.emplace_back(InQuery{
                        Operation::WRITE,
                        query.address,
                        query.data
                    });
                    block_it->dirty = false;
                }
            }
        } else { // Cache miss
            if (should_allocate(query.operation)) {
                // Выбираем жертву для вытеснения
                if (line.count >= associativity) {
                    auto victim_it = select_victim(line);
                    result.evicted = true;
                    result.evicted_tag = victim_it->tag;
                    
                    // Если блок "грязный" и политика Write-Back
                    if (victim_it->dirty && write_policy_ == WritePolicy::WRITE_BACK) {
                        result.out.emplace_back(InQuery{
                            Operation::WRITE,
                            (victim_it->tag << (offset_bits + index_bits)) | (index << offset_bits),
                            victim_it->data
                        });
                    }
                    
                    // Переиспользуем блок
                    victim_it->tag = tag;
                    victim_it->data = query.data;
                    victim_it->dirty = (query.operation == Operation::WRITE);
                    victim_it->valid = true;
                    
                    move_beg_block(line, victim_it);
                } else {
                    // Добавляем новый блок
                    line.cache_line.emplace_front(tag, query.data);
                    line.cache_line.front().dirty = (query.operation == Operation::WRITE);
                    line.count++;
                }
            } else {
                // Прямая запись в память без заведения блока
                result.out.push_back(query);
            }
            
            // Если это запись и политика Write-Through
            if (query.operation == Operation::WRITE && 
                write_policy_ == WritePolicy::WRITE_THROUGH) {
                result.out.push_back(query);
            }
        }
        
        return result;
    }

    void CacheL1::print_cache_state() const {
        std::cout << "\n=== Cache Configuration ===\n";
        std::cout << "Size:        " << size << " b\n";
        std::cout << "Block size:  " << block_size << " b\n";
        std::cout << "Associativity: " << associativity << "\n";
        std::cout << "Policy:      " 
                << (write_policy_ == WritePolicy::WRITE_BACK ? "Write-Back" : "Write-Through") << ", "
                << (alloc_policy_ == AllocationPolicy::WRITE_ALLOCATE ? "Write-Allocate" : "Read-Allocate") << ", "
                << (repl_policy_ == ReplacementPolicy::LRU ? "LRU" : "MRU")
                << "\n";

        std::cout << "\n=== Cache Contents ===\n";
        bool isEmpty = true;
        size_t total_blocks = 0;
        size_t dirty_blocks = 0;

        for (const auto& [index, line] : tag_store) {
            if (line.cache_line.empty()) continue;

            std::cout << "Set #" << std::setw(4) << std::left << index 
                    << " [" << line.count << "/" << associativity << " blocks]:\n";
            
            int block_num = 0;
            for (const auto& block : line.cache_line) {
                if (!block.valid) continue;
                
                isEmpty = false;
                total_blocks++;
                if (block.dirty) dirty_blocks++;

                std::cout << "  Block " << block_num++ << ": "
                        << "Tag=0x" << std::hex << std::setw(8) << std::setfill('0') 
                        << block.tag << std::dec << std::setfill(' ')
                        << " State: " << (block.dirty ? "Dirty" : "Clean")
                        << " Data: [";

                size_t show_count = block.data.valid_count;
                for (size_t i = 0; i < show_count; ++i) {
                    std::cout << block.data[i];
                    if (i < show_count - 1) std::cout << ", ";
                }
                std::cout << "]\n";
            }
        }

        if (isEmpty) {
            std::cout << "Cache is empty\n";
        }
        std::cout << "======================\n\n";
    }
}