#pragma once

#include <iostream>
#include <list>
#include <vector>
#include <optional>
#include <array> 
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <map>
#include <chrono>
#include <cstdint>

namespace Cache{
    enum class Operation { READ, WRITE};
    enum class WritePolicy {
        WRITE_BACK,    // Отложенная запись
        WRITE_THROUGH  // Сквозная запись
    };

    enum class AllocationPolicy {
        READ_ALLOCATE,  
        WRITE_ALLOCATE, 
        BOTH           
    };

    enum class ReplacementPolicy {
        LRU,  // Least Recently Used
        MRU,  // Most Recently Used
        RANDOM 
    };

    struct Data { 
        static constexpr size_t SIZE = 16; 
        std::array<int, SIZE> buffer;     
        size_t valid_count = 0;            // Число действительных элементов

        int& operator[](size_t index) {
            if (index >= SIZE) throw std::out_of_range("Data index out of range");
            return buffer[index];
        }

        const int& operator[](size_t index) const {
            if (index >= SIZE) throw std::out_of_range("Data index out of range");
            return buffer[index];
        }

        void fill(const int* src, size_t count) {
            if (count > SIZE) throw std::invalid_argument("Too much data");
            std::copy_n(src, count, buffer.begin());
            valid_count = count;
        }

        bool operator==(const Data& other) const {
            return buffer == other.buffer && valid_count == other.valid_count;
        }
    };

    struct CacheBlock {
        bool valid = false;       
        int tag{};
        bool dirty = false;
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
        size_t size = Data::SIZE;
    };

    struct OutQuery {
        bool hit = false;
        bool evicted = false; // если был вытеснен блок, сохраняем адрес и меняем флаг
        int evicted_tag = -1;
        std::vector<InQuery> out; // запросы, которые нужно передать дальше
        std::optional<Data> returned_data; // данные на чтение

        const Data* get_data() const {
            if (returned_data.has_value()) return &returned_data.value();
            return nullptr;
        }

        bool needs_memory_access() const {
            return !out.empty();
        }  
    };

    class Cache{ // безнаковые отметить u
    protected:
        size_t size;
        uint64_t block_size;        
        size_t associativity;     
        uint64_t address_bits;

        WritePolicy write_policy_ ;
        AllocationPolicy alloc_policy_;
        ReplacementPolicy repl_policy_;

        size_t num_lines; // число строк, считаем через размер и ассоциативность
        size_t offset_bits; // вспомогательные значения для адреса
        size_t index_bits;  
        size_t tag_bits;    

        std::unordered_map<size_t, CacheLine> tag_store;

    public:
        WritePolicy get_write_policy(){return write_policy_;}
        virtual auto query(InQuery const&) -> OutQuery = 0;  
    };
    
    class CacheL1 : public Cache {
    private:
        
        uint64_t get_tag(uint64_t address) const {
            return address >> (offset_bits + index_bits);
        }

        uint64_t get_index(uint64_t address) const {
            return (address >> offset_bits) & ((1 << index_bits) - 1);
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
                
                if (lru_block->dirty) { //если блок был изменен, то его нужно обновить или нужно сохранять все выт. блоки?
                    InQuery mem_op;
                    mem_op.operation = Operation::WRITE;
                    mem_op.address = (lru_block->tag << (offset_bits + index_bits)) | 
                                    (get_index(result.evicted_tag) << offset_bits);
                    mem_op.data = lru_block->data; 
                    result.out.push_back(mem_op);
                }
                
                lru_block->valid = true;
                lru_block->tag = tag;
                lru_block->dirty = false;
                lru_block->data = data;
                
                move_beg_block(line, lru_block);
            }
        }
    public:
        CacheL1(size_t size, uint64_t block_size, size_t associativity, uint64_t address_bits, WritePolicy wp, AllocationPolicy ap, ReplacementPolicy rp){
            this->size = size;
            this->block_size = block_size;
            this->associativity = associativity;
            this->address_bits = address_bits;

            this->write_policy_ = wp;
            this->alloc_policy_ = ap;
            this->repl_policy_ = rp;

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


        void handle_write(CacheBlock& block, const Data& data) {
            block.data = data;
            block.dirty = true;
            
            if (write_policy_ == WritePolicy::WRITE_THROUGH) {
                OutQuery result;
                InQuery mem_op{Operation::WRITE, static_cast<uint64_t>(block.tag), data};
                result.out.push_back(mem_op);
            }
        }

        bool should_allocate(Operation op) const {
            switch (alloc_policy_) {
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

        auto select_victim(CacheLine& line) -> std::list<CacheBlock>::iterator {
            switch (repl_policy_) {
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

        auto query(InQuery const&) -> OutQuery override;
        void print_cache_state() const;
    };

    class MemoryModel {
    private:
        std::unordered_map<uint64_t, Data> memory;

    public:
        OutQuery query(const InQuery& in) {
            OutQuery result;
            result.hit = true;
            uint64_t aligned_addr = in.address & ~(Data::SIZE - 1);
            
            if (in.operation == Operation::READ) {
                if (memory.count(aligned_addr)) {
                    result.returned_data = memory[aligned_addr];
                }
            } else if (in.operation == Operation::WRITE) {
                memory[aligned_addr] = in.data;  
            }
            return result;
        }
    };

}