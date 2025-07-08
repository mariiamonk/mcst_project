#include "../include/cache.hpp"

namespace Cache{
    auto CacheL1::query(InQuery const& query) -> OutQuery{
        OutQuery result;
        uint64_t tag = get_tag(query.address);
        uint64_t index = get_index(query.address);
        auto& line = tag_store[index];

        auto block_f = find_block(line, tag);
        if(block_f != line.cache_line.end()){ 
            //попали в кеш
            result.hit = true;
            move_beg_block(line, block_f);

            switch (query.operation)
            {
                case Operation::READ:
                    move_beg_block(line, block_f);

                    if (query.data.data) {
                        std::copy_n(block_f->data.data, block_f->data.count, query.data.data);
                    }
                    break;

                case Operation::WRITE:
                    move_beg_block(line, block_f);
                    if (query.data.count > 0) {
                        std::copy_n(query.data.data, query.data.count, block_f->data.data);
                        block_f->data.count = query.data.count;
                        block_f->state = true;
                    }
                    break;
                default:
                    break;
            }
        }else{
            InQuery mem_op;
            mem_op.operation = Operation::READ;
            mem_op.address = query.address & ~((1 << offset_bits) - 1); // выровненный адрес
                
            if (query.operation == Operation::WRITE) {
                mem_op.data = query.data;
            }
                
            result.out.push_back(mem_op);
                
            add_block(line, tag, query.data, result);
                
            // если это запись, помечаем блок как изменённый
            if (query.operation == Operation::WRITE && query.data.count > 0) {
                auto new_block = line.cache_line.begin();
                new_block->state = true;
            }
        }
        return result;
    }

    void CacheL1::print_cache_state() const {
        std::cout << "\n=== Cache State (showing only occupied lines) ===\n";
        std::cout << "Config: " << size << ", " << block_size << "B blocks, "
                << associativity << ", " << num_lines << " lines\n\n";

        for (const auto& [index, line] : tag_store) {
            if (line.cache_line.empty()) continue;

            std::cout << "Line " << index << " (" << line.count << "/" << associativity << "):\n";
            
            int block_num = 0;
            for (const auto& block : line.cache_line) {
                if (!block.valid) continue;
                
                std::cout << "  B" << block_num++ << ": "
                        << "Tag=0x" << std::hex << block.tag << std::dec
                        << "State:" << (block.state ? "[0]" : "[1]") << " [";
                
                for (int i = 0; i < block.data.count; ++i) {
                    std::cout << block.data.data[i];
                }
                if (block.data.count > 3) std::cout << "...";
                std::cout << "]\n";
            }
        }
        std::cout << "===================================\n";
    }
}