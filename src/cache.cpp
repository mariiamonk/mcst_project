#include "../include/cache.hpp"


namespace Cache{
    auto Cache::query(InQuery const& query) -> OutQuery {
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
                // Возвращаем данные из кэша
                result.returned_data = block_it->data;
            } else { // WRITE
                // Обновляем данные в кэше
                if (query.data.valid_count > 0) {
                    block_it->data.fill(query.data.buffer.data(), query.data.valid_count);
                }
                block_it->dirty = (write_policy_ != WritePolicy::WRITE_THROUGH);
                
                if (write_policy_ == WritePolicy::WRITE_THROUGH) {
                    // Сквозная запись - сразу в память
                    result.out.emplace_back(InQuery{
                        Operation::WRITE,
                        query.address,
                        block_it->data
                    });
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
                    if (query.operation == Operation::WRITE) {
                        victim_it->data = query.data;
                        victim_it->dirty = true;
                    } else {
                        // Для чтения помечаем как негрязный
                        victim_it->dirty = false;
                        // Запрашиваем данные из нижнего уровня
                        result.out.emplace_back(InQuery{
                            Operation::READ,
                            query.address,
                            {},
                            Data::SIZE
                        });
                    }
                    victim_it->valid = true;
                    
                    move_beg_block(line, victim_it);
                } else {
                    // Добавляем новый блок
                    line.cache_line.emplace_front(tag, query.data);
                    line.cache_line.front().valid = true;
                    line.cache_line.front().dirty = (query.operation == Operation::WRITE);
                    line.count++; // Увеличиваем счетчик
                    
                    if (query.operation == Operation::READ) {
                        result.out.emplace_back(InQuery{
                            Operation::READ,
                            query.address,
                            {},
                            Data::SIZE
                        });
                    }
                }
            } else {
                // Прямая запись в память без заведения блока
                result.out.push_back(query);
            }
        }
        
        return result;
    }


    void Cache::print_cache_state(){
        // std::cout << "\n=== Cache Configuration ===\n"
        // << "Size:        " << size << " b\n"
        // << "Block size:  " << block_size << " b\n"
        // << "Associativity: " << associativity << "\n"
        // << "Policy:      " 
        //         << (write_policy_ == WritePolicy::WRITE_BACK ? "Write-Back" : "Write-Through") << ", "
        //         << (alloc_policy_ == AllocationPolicy::WRITE_ALLOCATE ? "Write-Allocate" : "Read-Allocate") << ", "
        //         << (repl_policy_ == ReplacementPolicy::LRU ? "LRU" : "MRU")
        //         << "\n";

        std::cout << "\n=== Cache===\n";
        bool isEmpty = true;
        unsigned int total_blocks = 0;
        unsigned int dirty_blocks = 0;

        for (const auto& [index, line] : tag_store) {
            if (line.cache_line.empty()) continue;

            std::cout << "Set #" << std::setw(4) << std::left << index 
                    << " [" << line.count << "/" << associativity << " blocks]:\n";
            
            unsigned int block_num = 0;
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

                unsigned int show_count = block.data.valid_count;
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
    }
}