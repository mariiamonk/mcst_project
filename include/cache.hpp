#pragma once

#include <iostream>
#include <list>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <math.h>

namespace Cache{
    enum class Operation { READ, WRITE};

    struct Data {
        int* data;
        short capacity{16};
        short count{};

        Data() : data(new int[16]), count(0) {}
        ~Data() { delete[] data; }

        Data(const Data& other) : capacity(other.capacity), count(other.count) {
            data = new int[capacity];
            std::copy_n(other.data, count, data);
        }

        Data& operator=(const Data& other) {
            if (this != &other) {
                delete[] data;
                capacity = other.capacity;
                count = other.count;
                data = new int[capacity];
                std::copy_n(other.data, count, data);
            }
            return *this;
        }
        
        Data(Data&& other) noexcept 
            : data(other.data), capacity(other.capacity), count(other.count) {
            other.data = nullptr;
            other.count = 0;
        }
        
        Data& operator=(Data&& other) noexcept {
            if (this != &other) {
                delete[] data;
                data = other.data;
                capacity = other.capacity;
                count = other.count;
                other.data = nullptr;
                other.count = 0;
            }
            return *this;
        }
    };

    struct CacheBlock {
        bool valid = false;       
        int tag{};
        bool state = false;
        size_t hist{};
        Data data;

        CacheBlock(int tag, const Data& data) : tag(tag), data(data), valid(true) {}
    };

    struct CacheLine {
        std::list<CacheBlock> cache_line;
        size_t count{}; // для отслеживания ассоциативности
    }; 

    struct InQuery {
        Operation operation;
        uint64_t address;
        Data data;
    };

    struct OutQuery {
        bool hit = false;
        bool evicted = false; // если был вытеснен блок, сохраняем адрес и меняем флаг
        int evicted_tag = -1;
        std::vector<InQuery> out;
    };

    class Cache{
    protected:
        size_t size;
        uint64_t block_size;        
        size_t associativity;     
        uint64_t address_bits;

        size_t num_lines; // число строк, считаем через размер и ассоциативность
        size_t offset_bits; // вспомогательные значения для адреса
        size_t index_bits;  
        size_t tag_bits;    

        std::unordered_map<size_t, CacheLine> tag_store;

    public:
        virtual auto query(InQuery const&) -> OutQuery = 0;  
    };
    
    class CacheL1 : public Cache {
    private:
        
        uint64_t get_tag(uint64_t address) const {
            return address >> (offset_bits + index_bits);
        }
        
        uint64_t get_index(uint64_t address) const {
            return (address >> offset_bits) & (1 << index_bits) - 1;
        }
        
        uint64_t get_offset(uint64_t address) const {
            return address & ((1 << offset_bits) - 1);
        }

        auto find_block(CacheLine& line, uint64_t tag) { // возвращяем нужный блок
            return std::find_if(line.cache_line.begin(), line.cache_line.end(),
                [tag](const CacheBlock& block) { 
                    return block.valid && block.tag == tag; 
                });
        }
   
        // Метод для перемешения блока в начало списка
        void move_beg_block(CacheLine& line, std::list<CacheBlock>::iterator block_it) {
            if (block_it != line.cache_line.begin()) {
                line.cache_line.splice(line.cache_line.begin(), line.cache_line, block_it);
            }
        }

        void add_block(CacheLine& line, uint64_t tag, Data data, OutQuery& result) {
            if (line.count < associativity) { //есть место для записи
                CacheBlock new_block(tag, data);
                line.cache_line.push_front(new_block); // Добавляем в начало (как недавно использованный блок)
                line.count++;

            } else { //иначе по LRU вытесняем последний блок
                auto lru_block = std::prev(line.cache_line.end()); //последний элемент в списке 
                result.evicted = true;
                result.evicted_tag = lru_block->tag;
                
                if (lru_block->state) { //если блок был изменен, то его нужно обновить или нужно сохранять все выт. блоки?
                    InQuery mem_op;
                    mem_op.operation = Operation::WRITE;
                    mem_op.address = (lru_block->tag << (offset_bits + index_bits)) | 
                                    (get_index(result.evicted_tag) << offset_bits);
                    mem_op.data = lru_block->data; 
                    result.out.push_back(mem_op);
                }
                
                lru_block->valid = true;
                lru_block->tag = tag;
                lru_block->state = false;
                lru_block->data.count = 0;
                lru_block->data = data;
                
                move_beg_block(line, lru_block);
            }
        }
    public:
        CacheL1(size_t size, uint64_t block_size, size_t associativity, uint64_t address_bits) {
            this->size = size;
            this->block_size = block_size;
            this->associativity = associativity;
            this->address_bits = address_bits;

            this->offset_bits = static_cast<size_t>(log2(block_size));
            this->num_lines = size / (block_size * associativity);
            this->index_bits = static_cast<size_t>(log2(num_lines));
            this->tag_bits = address_bits - offset_bits - index_bits;

            this->tag_store.clear();
            for (size_t i = 0; i < num_lines; ++i) {
                CacheLine line;
                line.count = 0;
                this->tag_store[i] = line;
            }
        }

        auto query(InQuery const&) -> OutQuery override;
        void print_cache_state() const;
    };
}