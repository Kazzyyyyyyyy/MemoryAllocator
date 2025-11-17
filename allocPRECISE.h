#include <sys/mman.h> 
#include <stddef.h>
#include <stdio.h>
#include <iostream>

using namespace std; 

//ALLOC_PRECISE: ~70ns alloc time
template<const size_t MEM_SIZE = 16*1024*1024>
class MemAllocator {
    private: 

        struct Block {
            size_t size, offset;
            Block *next;
            bool free;
        };
        
        static constexpr    int8_t      SIZE_CLASS_NUM          = 20,
                                        MIN_BLOCK_SIZE          = 4;

        static constexpr    bool        FREE                    = true,
                                        NOT_FREE                = false;
        
        static constexpr    Block       *SIZE_CLASS_EMPTY       = nullptr;


        //                                   4        8        16       32       48       64       80       96       128      160      192      256      320      384      512      640      768      896      1024     1024<
        Block *sizeClasses[SIZE_CLASS_NUM] { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }; 
        void *memory;
        size_t offset = 0;

        void get_memory() {
            memory = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

            if(memory == MAP_FAILED) {
                perror("mmap");
                exit(1);
            }
        }
    
        inline int8_t get_size_class(const size_t size) const {
            if(size <= 4) return 0; 
            else if(size <= 8) return 1;
            else if(size <= 16) return 2;
            else if(size <= 32) return 3;
            else if(size <= 48) return 4;
            else if(size <= 64) return 5;
            else if(size <= 80) return 6;
            else if(size <= 96) return 7;
            else if(size <= 128) return 8;
            else if(size <= 160) return 9;
            else if(size <= 192) return 10;
            else if(size <= 256) return 11;
            else if(size <= 320) return 12;
            else if(size <= 384) return 13;
            else if(size <= 512) return 14;
            else if(size <= 640) return 15;
            else if(size <= 768) return 16;
            else if(size <= 896) return 17;
            else if(size <= 1024) return 18;
            else return 19;
        }
        
        Block *create_block(const size_t size) {
            Block *bl = (Block*)((char*)memory + offset); 

            bl->size = size; 
            bl->next = nullptr;

            offset += sizeof(Block) + size;
            bl->offset = offset; //start pos of next block

            return bl;
        }

        void remove_block_from_class(const Block *bl, const int8_t sizeClass) {
            Block *dummy = (Block*)((char*)memory + offset); 
            dummy->next = sizeClasses[sizeClass];
            Block *tmp = dummy; 

            while(tmp->next != nullptr) {
                if(tmp->next->offset == bl->offset) { //every block has a unique offset so we can use it for identification
                    tmp->next = tmp->next->next;
                    break; 
                }
                tmp = tmp->next;
            }

            sizeClasses[sizeClass] = dummy->next;
        }

        void add_block_to_class(Block *bl) {
            const int8_t sizeClass = get_size_class(bl->size);
            if(sizeClasses[sizeClass] == SIZE_CLASS_EMPTY)
                bl->next = nullptr; //gets done in the code alr, but better be sure
            else 
                bl->next = sizeClasses[sizeClass];
            
            sizeClasses[sizeClass] = bl;
        }   
     
        Block *split(Block *bl, const size_t size) {
            if(bl->size < MIN_BLOCK_SIZE + sizeof(Block) + size) //block big enough to split? 
                return nullptr; 

            const int8_t sizeClass = get_size_class(bl->size); 
            
            //create and init 'nbl' at the end of 'bl'
            Block *nbl = (Block*)((char*)memory + bl->offset - (sizeof(Block) + size));
            nbl->size = size; 
            nbl->offset = bl->offset;
            nbl->next = nullptr; 
            nbl->free = FREE;

            // remove 'bl' from sizeClasses 
            sizeClasses[sizeClass] = bl->next; //thats okay, because 'bl' is always the first member of the sizeClass

            //set new data for 'bl' after splitting
            bl->size -= (sizeof(Block) + size); 
            bl->offset -= (sizeof(Block) + size);
            bl->next = nullptr;  
            
            //sort 'bl' back into sizeClasses
            add_block_to_class(bl); 
            
            return nbl;
        }
        
        void coalescing(Block* bl) {
            if(bl->offset == offset) //cant be any Block infront of 'bl'
                return; 

            //get next block after 'bl'
            Block *nbl = (Block*)((char*)memory + bl->offset);
            if(!nbl->free) 
                return; 
            
            remove_block_from_class(nbl, get_size_class(nbl->size));
            
            //merge 'nbl' and 'bl'
            bl->offset = nbl->offset;
            bl->size += sizeof(Block) + nbl->size;
        }

        Block *first_fit(const size_t size) {
            const int8_t sizeClass = get_size_class(size); 
            if(sizeClasses[sizeClass] == SIZE_CLASS_EMPTY) 
                return nullptr; 
            
            Block *tmp = sizeClasses[sizeClass]; 

            //look for valid Block
            while(tmp != nullptr) {
                if(tmp->size >= size) {
                    remove_block_from_class(tmp, sizeClass); 
                    return tmp; 
                }
                tmp = tmp->next; 
            }

            //no Block found
            return nullptr;
        }

        Block *best_fit(const size_t size) {
            const int8_t sizeClass = get_size_class(size); 
            if(sizeClasses[sizeClass] == SIZE_CLASS_EMPTY) 
                return nullptr; 

            Block *tmp = sizeClasses[sizeClass],
                  *bestFit = nullptr; 
            
            //look for valid Block
            while(tmp != nullptr) {
                if((bestFit == nullptr || tmp->size < bestFit->size) && tmp->size >= size) {
                    bestFit = tmp; 
                    
                    if(bestFit->size == size)
                        break; 
                }
            }

            //no Block found
            if(bestFit == nullptr)
                return nullptr;

            remove_block_from_class(bestFit, sizeClass);
            return bestFit; 
        }

        Block *get_block(const size_t size) {
            Block *ret;
            int8_t sizeClass = get_size_class(size);

            //the higher the sizeClass, the more the size can vary.
            //for lower classes, with 4-32b flucatuation, first_fit is efficient enough,
            //for higher classes, with 64-128b fluctuation, best_fit is used to reduce fragmentation
            if(sizeClass <= 10) 
                ret = first_fit(size);
            else 
                ret = best_fit(size);

            //best -/ first_fit wasn't able to find a block
            //look in higher sizeClasses for a Block to split
            if(ret == nullptr) {
                sizeClass = get_size_class(size * 2); 
                for(; sizeClass < SIZE_CLASS_NUM; sizeClass++) {
                    if(sizeClasses[sizeClass] != SIZE_CLASS_EMPTY) {
                        ret = split(sizeClasses[sizeClass], size);
                        
                        if(ret != nullptr) 
                            break;
                    }
                }
            }
            
            //if best/first_fit & splitting failed, create a new block
            return (ret ? ret : create_block(size));
        }

    public:  
        MemAllocator() { get_memory(); }
        ~MemAllocator() { munmap(memory, MEM_SIZE); }

        void *mem_alloc(const size_t size) {
            Block *bl = get_block((size < MIN_BLOCK_SIZE ? MIN_BLOCK_SIZE : size));
            bl->free = NOT_FREE;
            return ((char*)memory + bl->offset - bl->size); //user memory
        }

        void mem_free(const void *ptr) {
            Block *bl = (Block*)((char*)ptr - sizeof(Block)); //ptr is where the data starts after the Block
            bl->free = FREE;

            coalescing(bl);
            add_block_to_class(bl);
        }
}; 
