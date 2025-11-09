#include <sys/mman.h> 
#include <stddef.h>
#include <stdio.h>
#include <iostream>

using namespace std; 

class Mem {
    private: 
        static constexpr bool       FREE                    = true,
                                    NOT_FREE                = false; 
        
        static constexpr size_t     MEM_SIZE                = 16*1024*1024,
                                    MIN_BLOCK_SIZE          = 16,
                                    DEFAULT_BLOCK_SIZE      = 16;

        struct Block {
            size_t size, offset; 
            bool free; 
            Block *prev; 
        }; 

        void *memory;  
        Block *head; // free block list
        size_t offset = 0;

        inline void init_allocator() {
            memory = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

            if(memory == MAP_FAILED) {
                perror("mmap");
                exit(1);
            }
        }

        Block *create_block(size_t size) {
            if(offset + sizeof(Block) + size > MEM_SIZE)
                throw logic_error("alloc error: no more memory space (solution not implemented yet)"); 

            Block *bl = (Block*)((char*)memory + offset);

            bl->size = size; 
            bl->prev = head;

            offset += sizeof(Block) + bl->size; 
            bl->offset = offset; 

            return bl;
        }

        Block *first_fit(size_t size) {
            //check all and find one that fits (or not)
            if(head->free) {
                Block *tmp = head; //head == newest Block

                while(tmp->prev != nullptr) {
                    if(tmp->size >= size && tmp->free) 
                        return tmp; 
                    
                    tmp = tmp->prev; 
                }

                if(head->prev == nullptr && head->size >= size && head->free)
                    return head;
            }

            return create_block(size);
        }

        inline size_t align_size(size_t size) const {
            return (size + 1); 
        }

    public:
        Mem() {
            init_allocator();

            //default block
            head = (Block*)((char*)memory + offset);
            
            head->size = DEFAULT_BLOCK_SIZE;
            head->free = FREE; 
            head->prev = nullptr;
            
            offset += sizeof(Block) + head->size;
            head->offset = offset; 
        }

        ~Mem() {
            munmap(memory, MEM_SIZE); //very sure thats enough and we dont need to free every pointer manually
        }

        void *mem_alloc(size_t size) {
            if(size % 2 != 0) 
                size = align_size(size); 

            Block *bl = first_fit((size < MIN_BLOCK_SIZE ? MIN_BLOCK_SIZE : size));
            bl->free = NOT_FREE;
            
            return ((char*)memory + bl->offset - bl->size); //user memory
        }

        void mem_free(void *ptr) {
            Block *bl = (Block*)((char*)ptr - sizeof(Block));
             
            if(head->free) {
                bl->free = FREE;
                bl->prev = head;
                head = bl; 
                return; 
            }

            head->free = FREE; 
        }
};

