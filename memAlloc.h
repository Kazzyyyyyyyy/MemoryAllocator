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
            size_t size, offset; 
            Block *next; 
        };

        static constexpr    uint8_t     ALIGNMENT              = 16; 

        static constexpr    uint8_t     HEADER_SIZE             = 32, // sizeof(Block) = 24b 
                                        MIN_USER_MEMORY         = 16, 
                                        SIZE_CLASS_NUM          = 7; 

        static constexpr    Block       *SIZE_CLASS_EMPTY       = nullptr; 
    
        Block *sizeClasses[SIZE_CLASS_NUM] { nullptr }; // contains only free Blocks
        void *memory;
        size_t offset = 0;

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

            if(mem != MAP_FAILED) 
                return mem; 
            
            perror("mmap");
            exit(1);
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

            if(tmp->offset == bl->offset) {
                sizeClasses[sizeClass] = tmp->next;  
                
                #ifdef TRACK_USE 
                    removeBlockFromClassDone++; 
                #endif 
                
                return; 
            }

            Block *tmp2 = tmp; 
            while(tmp2->next) {
                if(tmp2->next->offset == bl->offset) { // every block has a unique offset so we can use it for identification
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
            if(HEADER_SIZE + size + offset > MEM_SIZE) 
                return nullptr; 

            Block *bl = (Block*)((char*)memory + offset);

            bl->size = size; 
            bl->next = nullptr; 

            offset += HEADER_SIZE + size;
            bl->offset = offset; // start pos of next block

            #ifdef TRACK_USE 
                createBlockDone++;
            #endif 

            return bl;
        }


        Block *split(Block *bl, const size_t size) {
            // block big enough to split?
            if(bl->size < HEADER_SIZE + size + MIN_USER_MEMORY) {
                //std::cout << "split ret" << std::endl;
                return nullptr; 
            }

            const uint8_t sizeClass = get_size_class(bl->size); 
            
            // create and init nbl at the end of bl
            Block *nbl = (Block*)((char*)memory + bl->offset - (HEADER_SIZE + size));
            nbl->size = size; 
            nbl->offset = bl->offset;
            nbl->next = nullptr; 

            // remove bl from sizeClasses 
            sizeClasses[sizeClass] = bl->next; // thats okay, because bl is always the first member of the sizeClass

            // set new data for bl after splitting
            bl->size -= (HEADER_SIZE + size); 
            bl->offset -= (HEADER_SIZE + size);
            bl->next = nullptr;  
            
            // sort bl back into sizeClasses
            add_block_to_class(bl); 
            
            #ifdef TRACK_USE 
                splitDone++; 
            #endif 

            return nbl;
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
            
                return { bl->size, bl->offset }; 
            }

        #endif 

    public:

        MemAllocator() { memory = get_memory(MEM_SIZE); }
        ~MemAllocator() { munmap(memory, MEM_SIZE); }

        void *mem_alloc(size_t size) {
            size = size_control(size); 

            Block *bl = get_block(size);

            if(!bl) 
                return nullptr; 
 
            #ifdef TRACK_USE
                memAllocDone++;
            #endif 

            return (char*)bl + HEADER_SIZE; // user memory
        }
     
        bool mem_free(void *ptr) {
            // check for nullptr 
            if(!ptr) 
                return false;

            // check for foreign ptr 
            static const uintptr_t start = reinterpret_cast<uintptr_t>(memory); 
            static const uintptr_t end   = start + MEM_SIZE; 
                   const uintptr_t ptri  = reinterpret_cast<uintptr_t>(ptr); 

            if(ptri < start || ptri >= end) 
                return false; 

            Block *bl = (Block*)((char*)ptr - HEADER_SIZE);

            add_block_to_class(bl); 

            #ifdef TRACK_USE
                memFreeDone++;
            #endif
            
            return true; 
        }


        ///////////////////////////////////////// 
        void *mem_realloc(void *ptr, size_t size) {
            if(size == 0) {
                mem_free(ptr); 
                return nullptr; 
            }

            size = size_control(size); 
            
            if(!ptr) 
                return mem_alloc(size); 
            
            
            Block *bl = (Block*)((char*)ptr - HEADER_SIZE); 

            if(bl->size == size) 
                return ptr; 

            // shrink in place
            if(size < bl->size) {
                const size_t tmpOffset = bl->offset;
                bl->offset -= bl->size - size; // <--- BLOCK FRAGMENTATION HERE
                
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

