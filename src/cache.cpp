#include "../include/cache.hpp"


namespace Cache{
    void Cache::handle_write(CacheBlock& block, const Data& data) {
        block.data = data;
        block.dirty = true;
            
        if (_write_policy == WritePolicy::WRITE_THROUGH) {
            OutQuery result;
            InQuery mem_op{Operation::WRITE, static_cast<uint64_t>(block.tag), data};
            result.out.push_back(mem_op);
        }
    }

    bool Cache::should_allocate(Operation op) const {
        switch (_alloc_policy) {
            case AllocationPolicy::READ_ALLOCATE: 
                return op == Operation::READ;
            case AllocationPolicy::WRITE_ALLOCATE:
                return op == Operation::WRITE;
            case AllocationPolicy::BOTH:
                return true;
            default:
                return false;
        }
    }

    auto Cache::select_victim(CacheLine& line) -> std::list<CacheBlock>::iterator {
        switch (_repl_policy) {
            case ReplacementPolicy::LRU:
                return std::prev(line.cache_line.end());
            case ReplacementPolicy::MRU:
                return line.cache_line.begin();
            case ReplacementPolicy::RANDOM: {
                size_t index = rand() % line.cache_line.size();
                return std::next(line.cache_line.begin(), index);
            }
            default:
                    return std::prev(line.cache_line.end());
        }
    }

    void Cache::add_block(CacheLine& line, uint64_t tag, Data data, OutQuery& result) {
        if (line.count < _associativity) { //есть место для записи
            line.cache_line.emplace_front(tag, data);
            line.count++;
        } else { //иначе по LRU вытесняем последний блок
            auto lru_it = std::prev(line.cache_line.end());
            CacheBlock& evicted_block = *lru_it;

            result.evicted = true;
            result.evicted_tag = evicted_block.tag;

            //если блок изменен, сохраняем его
            if (evicted_block.dirty) {
                uint64_t address = (evicted_block.tag << (_offset_bits + _index_bits)) | 
                                (get_index(result.evicted_tag) << _offset_bits);
                result.out.push_back({Operation::WRITE, address, evicted_block.data});
            }

            evicted_block = {true, tag, false, std::move(data)};
            move_beg_block(line, lru_it);
        }
    }

    auto Cache::query(InQuery const& query) -> OutQuery {
        OutQuery result;
        uint64_t tag = get_tag(query.address);
        uint64_t index = get_index(query.address);
        auto& line = _tag_store[index];

        // Поиск блока в кэше
        auto block_it = find_block(line, tag);

        if (block_it != line.cache_line.end()) { // Cache hit
            result.hit = true;
            
            // Обновляем порядок в соответствии с политикой вытеснения
            if (_repl_policy != ReplacementPolicy::RANDOM) {
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
                block_it->dirty = (_write_policy != WritePolicy::WRITE_THROUGH);
                
                if (_write_policy == WritePolicy::WRITE_THROUGH) {
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
                if (line.count >= _associativity) {
                    auto victim_it = select_victim(line);
                    result.evicted = true;
                    result.evicted_tag = victim_it->tag;
                    
                    // Если блок "грязный" и политика Write-Back
                    if (victim_it->dirty && _write_policy == WritePolicy::WRITE_BACK) {
                        result.out.emplace_back(InQuery{
                            Operation::WRITE,
                            (victim_it->tag << (_offset_bits + _index_bits)) | (index << _offset_bits),
                            victim_it->data
                        });
                    }
                    
                    // Переиспользуем блок
                    victim_it->tag = tag;
                    if (query.operation == Operation::WRITE) {
                        auto& a = *victim_it;
                        a.data = query.data;
                        a.dirty = true;
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
                    auto& a = line.cache_line.emplace_front(tag, query.data);
                    a.valid = true;
                    a.dirty = (query.operation == Operation::WRITE);
                    line.count++;
                    
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

        std::cout << "\nCache:\n";
        bool isEmpty = true;
        unsigned int total_blocks = 0;
        unsigned int dirty_blocks = 0;

        for (const auto& [index, line] : _tag_store) {
            if (line.cache_line.empty()) continue;

            std::cout << "Set #" << std::setw(4) << std::left << index 
                    << " [" << line.count << "/" << _associativity << " blocks]:\n";
            
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