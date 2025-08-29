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
        size_t block_id = generate_block_id();
        
        if (line.count < _associativity) {
            line.cache_line.emplace_front(block_id, tag, data);
            line.count++;
        } else {
            auto lru_it = std::prev(line.cache_line.end());
            CacheBlock& evicted_block = *lru_it;

            result.evicted = true;
            result.evicted_tag = evicted_block.tag;

            if (evicted_block.dirty) {
                uint64_t address = (evicted_block.tag << (_offset_bits + _index_bits)) | 
                                (get_index(result.evicted_tag) << _offset_bits);
                result.out.push_back({Operation::WRITE, address, evicted_block.data});
            }

            size_t old_id = evicted_block.id;
            *lru_it = CacheBlock(old_id, true, tag, false, std::move(data));
            move_beg_block(line, lru_it);
        }
    }

    auto Cache::query(InQuery const& query) -> OutQuery {
        OutQuery result;
        
        size_t size_bytes = query.size;
        size_t elements = (size_bytes + sizeof(int) - 1) / sizeof(int);
        elements = std::min(elements, Data::SIZE);

        uint64_t tag = get_tag(query.address);
        uint64_t index = get_index(query.address);
        uint64_t offset = get_offset(query.address);
        auto& line = _tag_store[index];

        auto block_it = find_block(line, tag);

        if (block_it != line.cache_line.end()) { // Cache hit
            result.hit = true;
            
            if (_repl_policy != ReplacementPolicy::RANDOM) {
                move_beg_block(line, block_it);
            }

            if (query.operation == Operation::READ) {
                Data response;
                size_t elements_to_read = std::min(elements, Data::SIZE - offset);
                block_it->data.read_data(response.buffer.data(), elements_to_read, offset);
                response.valid_count = elements_to_read;
                result.returned_data = response;
            } else { // WRITE
                size_t elements_to_write = std::min(elements, Data::SIZE - offset);
                block_it->data.write_data(query.data.buffer.data(), elements_to_write, offset);
                block_it->dirty = (_write_policy == WritePolicy::WRITE_BACK);
                
                if (_write_policy == WritePolicy::WRITE_THROUGH) {
                    result.out.emplace_back(InQuery{
                        Operation::WRITE,
                        query.address,
                        block_it->data,
                        query.size
                    });
                    block_it->dirty = false;
                }
            }
        } else { // Cache miss
            if (should_allocate(query.operation)) {
                if (line.count >= _associativity) {
                    auto victim_it = select_victim(line);
                    result.evicted = true;
                    result.evicted_tag = victim_it->id;
                    
                    if (victim_it->dirty) {
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
                                        
                    *victim_it = CacheBlock(victim_it->id, true, tag, 
                       (query.operation == Operation::WRITE) && 
                       (_write_policy == WritePolicy::WRITE_BACK),
                       query.operation == Operation::WRITE ? query.data : Data{});
                    
                    // Для WRITE_THROUGH при записи сразу отправляем в память
                    if (query.operation == Operation::WRITE && 
                        _write_policy == WritePolicy::WRITE_THROUGH) {
                        victim_it->data = query.data;
                        victim_it->data.valid_count = std::min(query.data.valid_count, Data::SIZE);
                        
                        result.out.emplace_back(InQuery{
                            Operation::WRITE,
                            query.address,
                            query.data 
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
                    line.cache_line.emplace_front(generate_block_id(), tag, query.data);
                    auto& new_block = line.cache_line.front();
                    new_block.valid = true;
                    new_block.data.valid_count = Data::SIZE;
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
        std::cout << "\nCache:\n";
        bool isEmpty = true;
        unsigned int total_blocks = 0;
        unsigned int dirty_blocks = 0;

        for (const auto& [set_index, cache_line] : _tag_store) {
            if (cache_line.cache_line.empty()) continue;

            std::cout << "Set #" << set_index 
                    << " [" << cache_line.count << "/" << _associativity << " blocks]:\n";
            
            int block_counter = 0;
            for (const auto& cache_block : cache_line.cache_line) {
                if (!cache_block.valid) continue;
                
                isEmpty = false;
                total_blocks++;
                if (cache_block.dirty) dirty_blocks++;

                uint64_t full_address = (cache_block.tag << (_offset_bits + _index_bits)) | 
                                    (set_index << _offset_bits);

                std::cout << "  Block " << block_counter++
                        << "    Tag: 0x" << std::hex << cache_block.tag  
                        << "    Address: 0x" << full_address  
                        << "    State: " << (cache_block.dirty ? "Dirty" : "Clean")
                        << "    Data: [";
                
                for (size_t i = 0; i < cache_block.data.valid_count; ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << std::dec << cache_block.data[i];
                }
                std::cout << "]\n";
            }
        }

        if (isEmpty) {
            std::cout << "Cache is empty\n";
        }
    }
}