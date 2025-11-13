#include <sys/mman.h> 
#include <stddef.h>
#include <stdio.h>
#include <iostream>

using namespace std; 

//MODE: ALLOC_PRECISE (~XXns alloc time)
template<const size_t MEM_SIZE = 16*1024*1024> 
class Mem {
    private: 
        struct Block {
            size_t size, offset; 
            Block *next; 
            bool free; 
        }; 
        
        static constexpr    size_t      MIN_BLOCK_SIZE          = 4,
                                        SIZE_CLASS_NUM          = 20;

        static constexpr    bool        FREE                    = true,
                                        NOT_FREE                = false; 
        
        static constexpr    Block       *SIZE_CLASS_EMPTY       = nullptr; 


        //                                   4        8        16       32       48       64       80       96       128      160      192      256      320      384      512      640      768      896      1024     1024<
        Block *sizeClasses[SIZE_CLASS_NUM] { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }; 
        void *memory;
        size_t offset = 0;

        void init_allocator() {
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
        
        Block *split(Block *bl, const size_t size) {
            if(bl->size < MIN_BLOCK_SIZE + sizeof(Block) + size) //block big enough to split? 
                return nullptr; 
            
            size_t currSizeClass = get_size_class(bl->size); 
            //create and init 'nbl' at the end of 'bl'
            Block *nbl = (Block*)((char*)memory + bl->offset - (sizeof(Block) + size));
            nbl->size = size; 
            nbl->offset = bl->offset;
            nbl->next = nullptr; 
            nbl->free = FREE; 

            // remove 'bl' from sizeClasses 
            sizeClasses[currSizeClass] = bl->next; 

            //set new data for 'bl' where we splitted 'nbl' out of 
            bl->size -= (sizeof(Block) + size); 
            bl->offset -= (sizeof(Block) + size);
            bl->next = nullptr;  
            
            //sort 'bl' back into sizeClasses
            if(sizeClasses[currSizeClass] == SIZE_CLASS_EMPTY)
                sizeClasses[currSizeClass] = bl;
            else {
                bl->next = sizeClasses[currSizeClass];
                sizeClasses[currSizeClass] = bl;
            }

            return nbl; //return split block
        }

        Block *first_fit(const size_t size) {
            if(sizeClasses[get_size_class(size)] == SIZE_CLASS_EMPTY) 
                return nullptr; 
            
            Block *tmp = sizeClasses[get_size_class(size)]; 
            while(tmp != nullptr) {
                if(tmp->size >= size) {
                    remove_block_from_class(tmp, get_size_class(size)); 
                    return tmp; 
                }

                tmp = tmp->next; 
            }

            //no block found
            return nullptr;
        }

        Block *best_fit(const size_t size) {
            if(sizeClasses[get_size_class(size)] == SIZE_CLASS_EMPTY) 
                return nullptr; 

            
            Block *tmp = sizeClasses[get_size_class(size)],
                  *bestFit = nullptr; 

            while(tmp != nullptr) {
                if((bestFit == nullptr || tmp->size < bestFit->size) && tmp->size >= size) {
                    bestFit = tmp; 
                    
                    if(bestFit->size == size)
                        break; 
                }
            }

            if(bestFit == nullptr)
                return nullptr; //no fitting block found

            remove_block_from_class(bestFit, get_size_class(size));

            return bestFit; 
        }

        void remove_block_from_class(const Block *bl, const int8_t sc) {
            Block *dummy = (Block*)((char*)memory + offset); 
            dummy->next = sizeClasses[sc];
            Block *tmp = dummy; 

            while(tmp->next != nullptr) {
                if(tmp->next->offset == bl->offset) {
                    tmp->next = tmp->next->next;
                    break; 
                }

                tmp = tmp->next;
            }

            sizeClasses[sc] = dummy->next;
        }

        Block *get_block(const size_t size) {
            Block *ret;
            size_t currSizeClass = get_size_class(size);

            //the higher the sizeClass, the more the size can vary.
            //for lower classes, with 4-32b flucatuation, first_fit is efficient enough,
            //for higher classes, with 64-128b fluctuation, best_fit is useful to reduce fragmentation
            if(currSizeClass <= 10) 
                ret = first_fit(size);
            else 
                ret = best_fit(size);

            //best -/ first_fit wasn't able to find a fitting block
            //look in higher sizeClasses for a Block to split 
            if(ret == nullptr) {
                currSizeClass = get_size_class(size * 2); 
                for(; currSizeClass < SIZE_CLASS_NUM; currSizeClass++) {
                    if(sizeClasses[currSizeClass] != SIZE_CLASS_EMPTY) {
                        ret = split(sizeClasses[currSizeClass], size);
                        
                        if(ret != nullptr) 
                            break;
                    }
                }
            }
            
            //if best/first_fit & splitting failed, create a new block
            return (ret ? ret : create_block(size));
        }

        void coalescing(Block* bl) {
            if(bl->offset == offset) //cant be any Blocks infront of 'bl'
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

        void add_block_to_class(Block *bl) {
            const size_t sizeClass = get_size_class(bl->size);
            if(sizeClasses[sizeClass] == SIZE_CLASS_EMPTY)
                bl->next = nullptr; //gets done in the code alr, but better be sure
            else 
                bl->next = sizeClasses[sizeClass];
            
            sizeClasses[sizeClass] = bl;
        }

    public:  
        Mem() { init_allocator(); }
        ~Mem() { munmap(memory, MEM_SIZE); }

        void *mem_alloc(size_t size) {
            if(size % 2 != 0) 
                size++;
            
            Block *bl = get_block((size < MIN_BLOCK_SIZE ? MIN_BLOCK_SIZE : size));
            bl->free = NOT_FREE;
            return ((char*)memory + bl->offset - bl->size); //user memory
        }

        void mem_free(const void *ptr) {
            Block *bl = (Block*)((char*)ptr - sizeof(Block)); //ptr is at start of the data
            bl->free = FREE;

            coalescing(bl);
            add_block_to_class(bl); 
        }



        void test() {
            for(int i = 0; i < SIZE_CLASS_NUM; i++) {
                Block *tmp = sizeClasses[i]; 
                cout << i << ": ";
                while(tmp != nullptr) {
                    cout << tmp->size << " / ";  
                    tmp = tmp->next; 
                }
                cout << endl;
            }
            cout << endl;


            // for(int i = 0; i < SIZE_CLASS_NUM; i++) {
            //     int num = 0;  
            //     Block *tmp = sizeClasses[i]; 
            //     while(tmp != nullptr) {
            //         num++;
            //         tmp = tmp->next; 
            //     }
            //     cout << i << ": " << num << " / " << (sizeClasses[i] != nullptr ? sizeClasses[i]->size : 0) << endl;
            // }
            // cout << endl;
        }


}; 


//overengineered split (dont need the dummy remove algorithm): 
/*
        Block *split(Block *bl, size_t size) {
            if(bl->size < MIN_BLOCK_SIZE + sizeof(Block) + size) //big enough to split? 
                return nullptr; 
            
            //create and init new block at the end of the big block (bl)
            Block *nbl = (Block*)((char*)memory + bl->offset - (sizeof(Block) + size)); 
            nbl->size = size; 
            nbl->offset = bl->offset;
            nbl->next = nullptr; 
            nbl->free = FREE; 

            //remove bl from sizeClasses 
            Block *dummy = (Block*)((char*)memory + offset); //we dont increase offset so the block doesnt get "saved" and get overwritten when creating a new one
            dummy->next = sizeClasses[currSizeClass];
            Block *tmp = dummy; //idk if needed

            while(tmp->next != nullptr) {
                if(tmp->next->offset == bl->offset) {
                    tmp->next = tmp->next->next; // remove bl from list
                    break; 
                }
                tmp = tmp->next; 
            } 

            //update currSizeClass
            sizeClasses[currSizeClass] = dummy->next; 
            
            //set new data for the block (bl) we splitted nbl out of 
            bl->size -= (sizeof(Block) + size); 
            bl->offset -= (sizeof(Block) + size); 
            bl->next = nullptr;
            
            //set bl back into (a new) sizeClass
            currSizeClass = get_size_class(bl->size);
            if(sizeClasses[currSizeClass] == SIZE_CLASS_EMPTY)
                sizeClasses[currSizeClass] = bl;
            else {
                bl->next = sizeClasses[currSizeClass];
                sizeClasses[currSizeClass] = bl;
            }

            return nbl; //return split block 
        }
*/