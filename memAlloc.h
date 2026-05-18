#include <sys/mman.h>
#include <stddef.h>
#include <cstdint>
#include <stdio.h>
#include <iostream>
#include <cstring>
#include <span> 


#define DEBUG 
#define TRACK_USE

template<const size_t MEM_SIZE = 16*1024*1024> 
class MemAllocator {

    private: 
        #ifdef DEBUG 
            friend class AllocAndFree; 
        #endif

        struct Block {
            size_t  size;         
            size_t  *prevSize;
            Block   *next;
            bool    free; 
        };

        static constexpr    uint8_t     HEADER_SIZE             = 32, 
                                        MIN_USER_MEMORY         = 16, 
                                        ALIGNMENT               = 16,
                                        SIZE_CLASS_NUM          = 7;

        static constexpr    Block       *SIZE_CLASS_EMPTY       = nullptr; 
    

        Block       *sizeClasses[SIZE_CLASS_NUM] { SIZE_CLASS_EMPTY }; // sizeClasses array only contains free blocks
        void        *memory     = nullptr;
        size_t      *lastSize   = nullptr; 
        size_t      offset      = 0;
        uintptr_t   memPos      = 0;

        // all these get incremented only when the function was successful
        #ifdef TRACK_USE 
            size_t      createBlockDone                 =       0,
                        firstFitDone                    =       0,
                        memAllocDone                    =       0, 
                        memFreeDone                     =       0, 
                        removeBlockFromClassDone        =       0,
                        addBlockToClassDone             =       0,
                        splitDone                       =       0;

        #endif 

        void *get_memory(const size_t size) {
            void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

            if(mem == MAP_FAILED) {
                perror("mmap");
                exit(1);
            }
            
            memPos = reinterpret_cast<uintptr_t>(mem); // needed for position check arithmetic
            return mem;         
        }
        
        inline bool block_at_offset(Block *bl) {
            const uintptr_t blPos = reinterpret_cast<uintptr_t>((char*)bl + HEADER_SIZE + bl->size); 
            return blPos - memPos == offset; 
        } 

        inline uint8_t get_size_class(const size_t size) const {
            if(size <= 32)          return 0; 
            else if(size <= 64)     return 1; 
            else if(size <= 128)    return 2; 
            else if(size <= 256)    return 3; 
            else if(size <= 512)    return 4;
            else if(size <= 1024)   return 5;
            else                    return 6;
        }

        inline size_t size_control(size_t size) {
            if(size < MIN_USER_MEMORY) {
                size = MIN_USER_MEMORY; 
            }
            else {
                // ALIGNMENT = 16 (10000) and lets assume size = 18 (10010)
                //
                //     10010 +  (1111) = 100001 &   0000 (~1111)  =   100001 = 100000 = 32 
                size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1); //& 0000
            }

            return size; 
        }
        
        void remove_block_from_class(const Block *bl, const uint8_t sizeClass) {
            Block *tmp = sizeClasses[sizeClass]; 

            // check if wanted Block is first in his sizeClass
            if(tmp == bl) {
                sizeClasses[sizeClass] = tmp->next;  
                
                #ifdef TRACK_USE 
                    removeBlockFromClassDone++; 
                #endif 
                
                return; 
            }

            // go through sizeClass to find Block
            Block *tmp2 = tmp; 
            while(tmp2->next) {
                if(tmp2->next == bl) { 
                    tmp2->next = tmp2->next->next; 
                    break; 
                }

                tmp2 = tmp2->next;
            }

            sizeClasses[sizeClass] = tmp; 

            #ifdef TRACK_USE 
                removeBlockFromClassDone++; 
            #endif 
        }

        void add_block_to_class(Block *bl) {
            const uint8_t sizeClass = get_size_class(bl->size);
            if(sizeClasses[sizeClass] == SIZE_CLASS_EMPTY) 
                bl->next = nullptr; 
            else 
                bl->next = sizeClasses[sizeClass];
            
            sizeClasses[sizeClass] = bl;
            
            #ifdef TRACK_USE 
                addBlockToClassDone++; 
            #endif 
        }   
        
        Block *create_block(const size_t size) {
            // enough space to create new Block?
            if(MEM_SIZE - offset < HEADER_SIZE + size) 
                return nullptr; 

            Block *bl = (Block*)((char*)memory + offset);

            bl->size = size; 
            bl->next = nullptr; 
            bl->prevSize = lastSize; 

            lastSize = &bl->size; 

            offset += HEADER_SIZE + size;

            #ifdef TRACK_USE 
                createBlockDone++;
            #endif 

            return bl;
        }


        Block *split(Block *bl, const size_t size) {
            // block big enough to split?
            if(bl->size - MIN_USER_MEMORY < HEADER_SIZE + size) 
                return nullptr; 

            const uint8_t sizeClass = get_size_class(bl->size); 
            
            // create and init nbl at the end of bl
            Block *nbl = (Block*)((char*)bl + bl->size - size); // same as; Block *nbl = (Block*)((char*)bl + bl->size + HEADER_SIZE - (HEADER_SIZE + size));
            nbl->size = size; 
            nbl->next = nullptr; 
            nbl->free = true; 
            nbl->prevSize = &bl->size; 

            // remove bl from sizeClasses 
            sizeClasses[sizeClass] = bl->next; // thats okay, because bl is always the first member of the sizeClass

            // set new data for bl after splitting
            bl->size -= HEADER_SIZE + size; 
            bl->next = nullptr; 
            
            // if bl is the last created Block we need to update lastSize to &nbl->size because nbl will be the last Block after splitting
            if(block_at_offset(nbl)) { ///////////////////////////////
                lastSize = &nbl->size;
            }
            // if bl isnt the last created block - go to Block after nbl and change *prevSize to &nbl->size
            else {
                Block *sbl = (Block*)((char*)nbl + HEADER_SIZE + nbl->size); // Block after nbl 
                sbl->prevSize = &nbl->size;
            }
             
            // sort bl back into sizeClasses
            add_block_to_class(bl); 
            
            #ifdef TRACK_USE 
                splitDone++; 
            #endif 

            return nbl;
        }

        void right_coalescing(Block *bl) {
            // first block; nothing to coalesc infront 
            if(block_at_offset(bl)) 
                return; 

            Block *nbl = (Block*)((char*)bl + HEADER_SIZE + bl->size); 

            if(!nbl->free) 
                return; 

            // remove nbl because itll be part of bl after merging
            remove_block_from_class(nbl, get_size_class(nbl->size));

            // merge Blocks
            bl->size += HEADER_SIZE + nbl->size; 
        }

        Block *left_coalescing(Block *bl) {
            if(!bl->prevSize)
                return bl; 

            Block *pbl = (Block*)((char*)bl - *bl->prevSize - HEADER_SIZE); 

            if(!pbl->free) 
                return bl; 
            
            // remove pbl because size (and likely sizeClass) will change after merging
            remove_block_from_class(pbl, get_size_class(pbl->size));

            // merge Blocks
            pbl->size += HEADER_SIZE + bl->size;

            // return pbl as new Block 
            return pbl; 
        }

        Block *coalescing(Block *bl) { /////////////////// NOT TESTED YET
            const size_t blStartSize = bl->size;

            right_coalescing(bl); 
            bl = left_coalescing(bl);

            if(blStartSize < bl->size && !block_at_offset(bl)) {
                Block *nbl = (Block*)((char*)bl + HEADER_SIZE + bl->size); 
                nbl->prevSize = &bl->size;
            }
            
            return bl; 
        }
        
        Block *first_fit(const size_t size) {
            uint8_t sizeClass = get_size_class(size); 
            if(sizeClasses[sizeClass] == SIZE_CLASS_EMPTY && (sizeClass == SIZE_CLASS_NUM - 1 || sizeClasses[sizeClass + 1] == SIZE_CLASS_EMPTY)) 
                return nullptr; 

            for(; sizeClass < sizeClass + 1; sizeClass++) {
                Block *tmp = sizeClasses[sizeClass]; 

                // look for valid Block
                while(tmp != nullptr) {
                    if(tmp->size >= size) {
                        remove_block_from_class(tmp, sizeClass); 
                    
                        #ifdef TRACK_USE 
                            firstFitDone++;
                        #endif 
                    
                        return tmp; 
                    }

                    tmp = tmp->next; 
                }

                // no further class to increment to
                if(sizeClass == SIZE_CLASS_NUM - 1) 
                    break; 
            }

            // no Block found
            return nullptr;
        }

        //////////////////////////////// make nicer 
        Block *get_block(const size_t size) { 
            Block *ret = first_fit(size);
 
            // first_fit wasn't able to find a block
            // look in higher sizeClasses for a Block to split
            if(!ret) { 
                for(uint8_t sizeClass = get_size_class(size) + 1; sizeClass < SIZE_CLASS_NUM; sizeClass++) {
                    if(sizeClasses[sizeClass] != SIZE_CLASS_EMPTY) {
                        ret = split(sizeClasses[sizeClass], size);
                        
                        if(ret) 
                            break;
                    }
                }
            }

            return (ret ? ret : create_block(size));
        }
        

        #ifdef DEBUG 
            inline size_t fast_block_size() const {
                //std::cout << sizeof(Block) << std::endl; 

                return HEADER_SIZE; 
            }

            void print_size_classes() {
                for(int i = 0; i < SIZE_CLASS_NUM; i++) {
                    std::cout << std::endl << i << " - ";
                    if(sizeClasses[i] == SIZE_CLASS_EMPTY)  {
                        std::cout << "empty";
                        continue;
                    }


                    Block *tmp = sizeClasses[i]; 
 
                    while(tmp != nullptr) {
                        std::cout << tmp->size << ", "; 
                        tmp = tmp->next; 
                    }
                }

                std::cout << std::endl << std::endl;  
            }

            std::pair<size_t, size_t> get_block_data(void *ptr) {
                Block *bl = (Block*)((char*)ptr - HEADER_SIZE);
            
                return { bl->size, 0 }; 
            }  

            size_t get_prevSize(void *ptr) {
                Block *bl = (Block*)((char*)ptr - HEADER_SIZE);

                if(!bl->prevSize) 
                    return 0; 

                return *bl->prevSize; 
            }

            void print_prevSize(void *ptr) {
                Block *bl = (Block*)((char*)ptr - HEADER_SIZE);

                std::cout << "size; " << bl->size << " - "  << (bl->prevSize ? *bl->prevSize : 0) << std::endl; 
            } 
          
        #endif 


    public:

        MemAllocator()  { memory = get_memory(MEM_SIZE); }
        ~MemAllocator() { munmap(memory, MEM_SIZE); }

        void *mem_alloc(size_t size) {
            size = size_control(size); 

            Block *bl = get_block(size);
            
            #ifdef TRACK_USE
                memAllocDone++;
            #endif 
            
            if(!bl) 
                return nullptr;

            bl->free = false;

            return (char*)bl + HEADER_SIZE; // user memory
        }
     
        bool mem_free(void *ptr) {
            // check for nullptr 
            if(!ptr) 
                return false;

            // check for foreign ptr 
            static const uintptr_t end   = memPos + MEM_SIZE; 
                   const uintptr_t ptri  = reinterpret_cast<uintptr_t>(ptr); 

            if(ptri < memPos || ptri >= end) 
                return false; 

            Block *bl = (Block*)((char*)ptr - HEADER_SIZE);

            if(bl->free) 
                return false; 

            bl->free = true; 

            bl = coalescing(bl); 
            add_block_to_class(bl); 

            #ifdef TRACK_USE
                memFreeDone++;
            #endif
            
            return true; 
        }


        ///////////////////////////////////////// ill rework this when coalescing is done 
        void *mem_realloc(void *ptr, size_t size) {
            if(size == 0) {
                mem_free(ptr); 
                return nullptr; 
            }

            size = size_control(size); 
            
            if(!ptr) 
                return mem_alloc(size); 
            
            // get Block
            Block *bl = (Block*)((char*)ptr - HEADER_SIZE); 

            if(bl->size == size) 
                return ptr; 

            // shrink in place ///////////////////////////////////////////////
            if(size < bl->size) {
                const size_t tmpOffset = bl->offset;
                bl->offset -= bl->size - size; 
                
                if(tmpOffset == offset) 
                    offset = bl->offset;

                bl->size = size; 
                return ptr; 
            }

            // grow in place
            if(bl->offset == offset) {
                bl->offset += size - bl->size; 
                offset = bl->offset; 
                bl->size = size; 
                
                return ptr; 
            }

            // realloc in new block 
            Block *nbl = create_block(size); 
            
            std::memcpy((char*)nbl + HEADER_SIZE, ptr, bl->size); 
            
            mem_free(ptr); 

            return (char*)nbl + HEADER_SIZE; 
        }
}; 

