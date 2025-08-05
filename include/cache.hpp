#pragma once

#include <iostream>
#include <list>
#include <vector>
#include <optional>
#include <array> 
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

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

    class Data { 
    public:
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
        
        void print_data() const {
            if (valid_count == 0) {
                std::cout << "<no data>";
                return;
            }

            for (size_t i = 0; i < valid_count; ++i) {
                std::cout << buffer[i];
                if (i < valid_count - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;
        }
    };

    struct CacheBlock {
        bool valid = false;       
        unsigned long int tag{};
        bool dirty = false;
        Data data;

        CacheBlock(bool v, uint64_t t, bool d, Data dt): valid(v), tag(t), dirty(d), data(std::move(dt)) {
                if (dt.valid_count > 0) {
                    data.valid_count = dt.valid_count;
                } else {
                    data.valid_count = Data::SIZE;
                }
        }
    
        CacheBlock(uint64_t t, const Data& d) : tag(t), data(d), valid(true) {}
    };

    struct CacheLine {
        std::list<CacheBlock> cache_line;
        unsigned int count{}; // для отслеживания ассоциативности
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
    
    class Cache{
    private:
        size_t _size;
        size_t _block_size;        
        size_t _associativity;     
        uint64_t _address_bits;

        WritePolicy _write_policy ;
        AllocationPolicy _alloc_policy;
        ReplacementPolicy _repl_policy;

        size_t _num_lines; // число строк, считаем через размер и ассоциативность
        size_t _offset_bits; // вспомогательные значения для адреса
        size_t _index_bits;  
        size_t _tag_bits;    

        std::unordered_map<size_t, CacheLine> _tag_store;
    public:
        Cache(size_t size, uint64_t block_size, size_t associativity, uint64_t address_bits, WritePolicy wp, AllocationPolicy ap, ReplacementPolicy rp) 
        : _size(size), _block_size(block_size), _associativity(associativity), _address_bits(address_bits),
          _write_policy(wp), _alloc_policy(ap), _repl_policy(rp),
          _num_lines(size / (block_size * associativity)),
          _offset_bits(static_cast<size_t>(log2(block_size))),  _index_bits(static_cast<size_t>(log2(_num_lines))), _tag_bits(address_bits - _offset_bits - _index_bits){

            _tag_store.clear();
            CacheLine line;
            for (size_t i = 0; i < _num_lines; ++i) {
                
                line.count = 0;
                _tag_store[i] = line;
            }
        }

        auto query(InQuery const&) -> OutQuery;

        uint64_t get_tag(uint64_t address) const {
            return address >> (_offset_bits + _index_bits);
        }

        uint64_t get_index(uint64_t address) const {
            return (address >> _offset_bits) & ((1 << _index_bits) - 1);
        }
        
        uint64_t get_offset(uint64_t address) const {
            return address & ((1 << _offset_bits) - 1);
        }

        auto find_block(CacheLine& line, uint64_t tag) { // возвращaем нужный блок
            return std::find_if(line.cache_line.begin(), line.cache_line.end(),
                [tag](const CacheBlock& block) { 
                    return block.valid && block.tag == tag; 
                });
        }

        auto get_write_policy(){return _write_policy;}
        std::unordered_map<size_t, CacheLine>& get_tag_store() { return _tag_store; }

        void handle_write(CacheBlock& block, const Data& data);

        bool should_allocate(Operation op) const;
        auto select_victim(CacheLine& line) -> std::list<CacheBlock>::iterator;

        void print_cache_state();
    private:
        // Метод для перемешения блока в начало списка
        void move_beg_block(CacheLine& line, std::list<CacheBlock>::iterator block_it) {
            if (block_it != line.cache_line.begin()) {
                line.cache_line.splice(line.cache_line.begin(), line.cache_line, block_it);
            }
        }

        void add_block(CacheLine& line, uint64_t tag, Data data, OutQuery& result);
    };
}