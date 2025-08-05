#include "../include/cache.hpp"


namespace Cache{
    void Cache::handle_write(CacheBlock& block, const Data& data) {
        block.data = data;
        block.dirty = (_write_policy == WritePolicy::WRITE_BACK);
        
        if (_write_policy == WritePolicy::WRITE_THROUGH) {
            OutQuery result;
            uint64_t address = (block.tag << (_offset_bits + _index_bits)) | 
                            (get_index(block.tag) << _offset_bits);
            result.out.push_back({Operation::WRITE, address, data});
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

        auto block_it = find_block(line, tag);

        if (block_it != line.cache_line.end()) { // Cache hit
            result.hit = true;
            
            if (_repl_policy != ReplacementPolicy::RANDOM) {
                move_beg_block(line, block_it);
            }

            if (query.operation == Operation::READ) {
                result.returned_data = block_it->data;
            } else { // WRITE
                if (query.data.valid_count > 0) {
                    block_it->data.fill(query.data.buffer.data(), query.data.valid_count);
                }
                
                if (_write_policy == WritePolicy::WRITE_THROUGH) {
                    // Для сквозной записи сразу отправляем данные в память
                    result.out.emplace_back(InQuery{
                        Operation::WRITE,
                        query.address,
                        block_it->data
                    });
                    block_it->dirty = false; 
                } else {
                    // Для отложенной записи помечаем блок как "грязный"
                    block_it->dirty = true;
                }
            }
        } else { // Cache miss
            if (should_allocate(query.operation)) {
                if (line.count >= _associativity) {
                    auto victim_it = select_victim(line);
                    result.evicted = true;
                    result.evicted_tag = victim_it->tag;
                    
                    if (victim_it->dirty) {
                        // Если политика Write-Back и блок dirty - записываем в память
                        if (_write_policy == WritePolicy::WRITE_BACK) {
                            uint64_t evicted_addr = (victim_it->tag << (_offset_bits + _index_bits)) | 
                                                (index << _offset_bits);
                            result.out.emplace_back(InQuery{
                                Operation::WRITE,
                                evicted_addr,
                                victim_it->data
                            });
                        }
                    }
                    
                    // Заменяем блок
                    victim_it->tag = tag;
                    victim_it->data = query.operation == Operation::WRITE ? query.data : Data{};
                    
                    if (query.operation == Operation::READ) {
                        victim_it->data.valid_count = Data::SIZE;
                    }
                                        
                    victim_it->valid = true;
                    
                    victim_it->dirty = (query.operation == Operation::WRITE) && 
                                    (_write_policy == WritePolicy::WRITE_BACK);
                    
                    // Для WRITE_THROUGH при записи сразу отправляем в память
                    if (query.operation == Operation::WRITE && 
                        _write_policy == WritePolicy::WRITE_THROUGH) {
                        result.out.emplace_back(InQuery{
                            Operation::WRITE,
                            query.address,
                            victim_it->data
                        });
                        victim_it->dirty = false;
                    }
                    
                    // Для чтения запрашиваем данные из памяти
                    if (query.operation == Operation::READ) {
                        result.out.emplace_back(InQuery{
                            Operation::READ,
                            query.address,
                            {},
                            Data::SIZE
                        });
                    }
                    
                    move_beg_block(line, victim_it);
                } else {
                    // Добавляем новый блок
                    line.cache_line.emplace_front(tag, query.data);
                    auto& new_block = line.cache_line.front();
                    new_block.valid = true;
                    new_block.dirty = (query.operation == Operation::WRITE) && 
                                    (_write_policy == WritePolicy::WRITE_BACK);
                    line.count++;

                    if (query.operation == Operation::READ) {
                        new_block.data.valid_count = Data::SIZE;
                    }
                    
                    // Для WRITE_THROUGH при записи сразу отправляем в память
                    if (query.operation == Operation::WRITE && 
                        _write_policy == WritePolicy::WRITE_THROUGH) {
                        result.out.emplace_back(InQuery{
                            Operation::WRITE,
                            query.address,
                            new_block.data
                        });
                        new_block.dirty = false;
                    }
                    
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
                //Прямая запись в память
                if (query.operation == Operation::WRITE) {
                    result.out.push_back(query);
                } else {
                    result.out.emplace_back(InQuery{
                        Operation::READ,
                        query.address,
                        {},
                        Data::SIZE
                    });
                }
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

                uint64_t full_addr = (block.tag << (_offset_bits + _index_bits)) | (index << _offset_bits);

                std::cout << "  Block " << block_num++ << ": "
                        << "Tag=0x" << std::hex << std::setw(8) << std::setfill('0') 
                        << block.tag << std::dec << " (addr 0x" << std::hex << full_addr << ")"
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