#include <sys/mman.h> 
#include <stddef.h>
#include <stdio.h>
#include <iostream>

using namespace std; 

//MODE: ALLOC_FAST (~25ns alloc time)
template<const size_t MEM_SIZE = 16*1024*1024> 
class Mem {
    private:

        struct Block {
            size_t size, offset; 
            Block *next; 
        }; 

        //                      16,      32,      64,      128,     256,     512,     1024     1024
        Block *sizeClasses[8] { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }; 
        void *memory;
        size_t offset = 0;
        int8_t currSizeClass = 0; 

        void init_allocator() {
            memory = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

            if(memory == MAP_FAILED) {
                perror("mmap");
                exit(1);
            }
        }

        inline int8_t get_curr_size_class(size_t size) const {
            if(size <= 16) return 0; 
            else if(size <= 32) return 1; 
            else if(size <= 64) return 2; 
            else if(size <= 128) return 3; 
            else if(size <= 256) return 4; 
            else if(size <= 512) return 5;
            else if(size <= 1024) return 6;
            else return 7;
        }
  
        Block *create_block(size_t size) {
            Block *bl = (Block*)((char*)memory + offset); 

            bl->size = size; 
            bl->next = nullptr; 

            offset += sizeof(Block) + size;
            bl->offset = offset; //start pos of next block

            return bl;
        }

        Block *first_fit(size_t size) {
            currSizeClass = get_curr_size_class(size);

            if(sizeClasses[currSizeClass] != nullptr) {
                Block *tmp = sizeClasses[currSizeClass];
                sizeClasses[currSizeClass] = tmp->next;
                return tmp; 
            }

            return create_block(size);
        }

    public:
        Mem() { init_allocator(); }

        ~Mem() { munmap(memory, MEM_SIZE); }

        void *mem_alloc(size_t size) {
            Block *bl = first_fit(size);
            return ((char*)memory + bl->offset - bl->size); //user memory
        }

        void mem_free(void *ptr) {
            Block *bl = (Block*)((char*)ptr - sizeof(Block));
        
            currSizeClass = get_curr_size_class(bl->size); 

            if(sizeClasses[currSizeClass] == nullptr)
                sizeClasses[currSizeClass] = bl; 
            else {
                bl->next = sizeClasses[currSizeClass]; 
                sizeClasses[currSizeClass] = bl; 
            }
            
            return; 
        }
};

