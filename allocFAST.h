#include <sys/mman.h> 
#include <stddef.h>
#include <cstdint>
#include <stdio.h>
#include <iostream>

using namespace std; 

//ALLOC_FAST: ~25ns alloc time
template<const size_t MEM_SIZE = 16*1024*1024> 
class MemAllocator {
    private:
    
        struct Block {
            size_t size, offset; 
            Block *next; 
        };

        static constexpr    uint8_t     SIZE_CLASS_NUM          = 8,
                                        MIN_BLOCK_SIZE          = 4;

        static constexpr    Block       *SIZE_CLASS_EMPTY       = nullptr; 
        
        //                                   16,      32,      64,      128,     256,     512,     1024     1024<
        Block *sizeClasses[SIZE_CLASS_NUM] { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }; // contains only free Blocks
        void *memory;
        size_t offset = 0;

        void get_memory() {
            memory = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

            if(memory == MAP_FAILED) {
                perror("mmap");
                exit(1);
            }
        }

        inline uint8_t get_size_class(const size_t size) const {
            if(size <= 16) return 0; 
            else if(size <= 32) return 1; 
            else if(size <= 64) return 2; 
            else if(size <= 128) return 3; 
            else if(size <= 256) return 4; 
            else if(size <= 512) return 5;
            else if(size <= 1024) return 6;
            else return 7;
        }
  
        Block *create_block(const size_t size) {
            Block *bl = (Block*)((char*)memory + offset);

            bl->size = size; 
            bl->next = nullptr; 

            offset += sizeof(Block) + size;
            bl->offset = offset; //start pos of next block

            return bl;
        }

        Block *first_fit(const size_t size) {
            const uint8_t sizeClass = get_size_class(size);

            if(sizeClasses[sizeClass] == SIZE_CLASS_EMPTY || sizeClasses[sizeClass]->size < size) 
                return create_block(size); 

            //remove Block from sizeClass and return
            Block *ret = sizeClasses[sizeClass]; 
            sizeClasses[sizeClass] = sizeClasses[sizeClass]->next; 
            
            return ret; 
        }

    public:
        MemAllocator() { get_memory(); }
        ~MemAllocator() { munmap(memory, MEM_SIZE); }

        void *mem_alloc(const size_t size) {
            Block *bl = first_fit((size < MIN_BLOCK_SIZE ? MIN_BLOCK_SIZE : size));
            return ((char*)memory + bl->offset - bl->size); //user memory
        }

        void mem_free(const void *ptr) {
            Block *bl = (Block*)((char*)ptr - sizeof(Block));

            //add block to sizeClass
            const uint8_t sizeClass = get_size_class(bl->size);

            if(sizeClasses[sizeClass] == nullptr)
                sizeClasses[sizeClass] = bl; 
            else {
                bl->next = sizeClasses[sizeClass]; 
                sizeClasses[sizeClass] = bl;
            }
        }
};

