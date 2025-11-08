#include <sys/mman.h> 
#include <stddef.h>
#include <stdio.h>
#include <iostream>

using namespace std; 

class Mem {
    private: 
        static constexpr bool FREE                      = true,
                              NOT_FREE                  = false; 
        
        static constexpr size_t MIN_BLOCK_SIZE          = 4,
                                MEM_SIZE                = 16*1024*1024,
                                DEFAULT_BLOCK_SIZE      = 16; 

        struct Block {
            size_t size, offset; 
            bool free; 
            Block *prev, *next; 
        }; 

        void *memory;  
        Block *head; // always the newest Block
        size_t offset = 0, freeBlocks = 0;

        void init_allocator() {
            memory = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

            if(memory == MAP_FAILED) {
                perror("mmap");
                exit(1);
            }
        }

        Block *create_block(size_t &size) {
            Block *bl = (Block*)((char*)memory + offset);

            bl->size = size; 
            bl->prev = head;
            bl->next = nullptr;
            
            head->next = bl; 
            head = bl; 

            offset += sizeof(Block) + bl->size; 
            head->offset = offset; 

            return head;
        }

        Block *first_fit(size_t size) {
            if(freeBlocks > 0) {
                Block *tmp = head; 
                while(tmp != nullptr) {
                    if(tmp->size >= size && tmp->free)
                        return tmp; 
                    
                    tmp = tmp->prev; 
                }
            }

            return create_block(size);
        }
        

    public:
        Mem() {
            init_allocator();

            //default block
            head = (Block*)((char*)memory + offset);
            
            head->size = DEFAULT_BLOCK_SIZE; 
            head->free = FREE; 
            head->prev = nullptr; 
            head->next = nullptr; 
            
            offset += sizeof(Block) + head->size; 
            head->offset = offset; 
            freeBlocks++; 
        }

        ~Mem() {
            munmap(memory, MEM_SIZE); //very sure thats enough and we dont need to free every pointer manually
        }

        void *mem_alloc(size_t size) {
            Block *bl = first_fit(size);
            bl->free = NOT_FREE;

            return ((char*)memory + bl->offset - bl->size); //user memory
        }

        void mem_free(void *ptr) {
            Block *bl = (Block*)((char*)ptr - sizeof(Block));
            bl->free = FREE;
            freeBlocks++; 
        }
};

