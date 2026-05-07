#include <iostream> 
#include "../memAlloc.h" 
#include "testData.cpp"
#include <vector>
#include <set>
#include <string>
#include <sstream>
#include <cstdint>
#include <memory> 
#include <random> 
#include <cmath> 

using namespace std; 

class AllocAndFree {
    private: 
        friend class Tests;

        size_t     randSeed; 

        size_t     FAST_BLOCK_SIZE; 
        int        CHAR_TEST_AMNT; 
        int        STRING_TEST_AMNT;

        inline size_t ran(size_t min = 0, size_t max = SIZE_MAX) const {
            static random_device rd;
            static mt19937 gen(rd());
            uniform_int_distribution<> dist(min, max);
            return dist(gen);
        }

        inline MemAllocator<Data::MEM_SIZE> get_alloc_instance() const {
            return MemAllocator<Data::MEM_SIZE>();
        }

        pair<bool, int> pure_alloc() {
            MemAllocator mem = get_alloc_instance(); 
            set<char*> s; 

            for(int i = 0; i < CHAR_TEST_AMNT; i++) {
                char *x = (char*)mem.mem_alloc(sizeof(char));
                
                // check for already used address 
                if(s.find(x) != s.end())
                    return { false, 0 }; 

                s.insert(x); 
            }
            
            // all allocs completed?
            if(mem.memAllocDone != CHAR_TEST_AMNT) 
                return { false, 1 }; 
            
            return { true, -1 }; 
        }

        pair<bool, int> alloc_and_free() {
            MemAllocator mem = get_alloc_instance(); 
            vector<char*> v; 

            for(int i = 0; i < CHAR_TEST_AMNT; i++) 
                v.push_back((char*)mem.mem_alloc(sizeof(char))); 
            
            for(char *x : v) 
                mem.mem_free(x); 
               
            if(mem.memFreeDone != CHAR_TEST_AMNT)
                return { false, 0 }; 
            
            return { true, -1 }; 
        }
        
        pair<bool, int> trade_alloc_and_free() {
            MemAllocator mem = get_alloc_instance(); 
            
            char *curr = (char*)mem.mem_alloc(sizeof(char)); // this address should be the only one we get in this whole test
            char *original = curr; 
            
            for(int i = 0; i < CHAR_TEST_AMNT; i++) {
                mem.mem_free(curr); // free => size class 
                curr = (char*)mem.mem_alloc(sizeof(char)); // new alloc with same size => should give the same address

                if(original != curr)
                    return { false, 0 }; 
            }

            // check if all frees and allocs where sucessful 
            if(mem.memFreeDone != CHAR_TEST_AMNT || mem.memAllocDone != CHAR_TEST_AMNT + 1)
        	    return { false, 1};

            return { true, -1 }; 
        }

        pair<bool, int> invalid_ptr_free() {
            MemAllocator mem = get_alloc_instance(); 

            unique_ptr<char> a(nullptr); 
            if(mem.mem_free(a.get()))
                return { false, 0 };  

            unique_ptr<char> b(new char()); 
            if(mem.mem_free(b.get())) 
                return { false, 1 }; 

            if(mem.memFreeDone > 0) 
                return { false, 2 }; 

            return { true, -1 }; 
        }

        pair<bool, int> random_type_alloc() {
            MemAllocator mem = get_alloc_instance(); 
            size_t byteAlloc = 0; 

            for(int i = 0; i < STRING_TEST_AMNT; i++) {
                switch(ran(0, 4)) {
                    case 0: 
                        mem.mem_alloc(sizeof(char));
                        byteAlloc += FAST_BLOCK_SIZE + mem.MIN_USER_MEMORY; // char < MIN_BLOCK_SIZE
                        break; 

                    case 1: 
                        mem.mem_alloc(sizeof(int));
                        byteAlloc += FAST_BLOCK_SIZE + mem.MIN_USER_MEMORY; // char < MIN_BLOCK_SIZE; 
                        break;
                    
                    case 2: 
                        mem.mem_alloc(sizeof(size_t));
                        byteAlloc += FAST_BLOCK_SIZE + mem.MIN_USER_MEMORY; // char < MIN_BLOCK_SIZE; 
                        break;
                    
                    case 3: 
                        mem.mem_alloc(24); 
                        byteAlloc += FAST_BLOCK_SIZE + 32; 
                        break;
                    
                    case 4:     
                        mem.mem_alloc(sizeof(string));
                        byteAlloc += FAST_BLOCK_SIZE + sizeof(string); 
                        break;
                }
            }
            
            if(mem.offset != byteAlloc) 
                return { false, 0 };
            
            return { true, -1 }; 
        }

        pair<bool, int> max_alloc_and_free() { 
            MemAllocator mem = get_alloc_instance();
            
            static const size_t maxAlloc = Data::MEM_SIZE - FAST_BLOCK_SIZE; 
            size_t byteAlloc = 0,
                   freed     = 0; 

            vector<void*> v; 

            for(int i = 0; i < 10'000'000; i++) {
                switch(ran(0, 1)) {
                    // alloc
                    case 0:{ 
                        // try alloc even though full
                        if(byteAlloc == Data::MEM_SIZE) {
                            if(mem.mem_alloc(maxAlloc)) 
                                return { false, 0 }; 

                            break; 
                        }

                        // allocate 
                        v.push_back(mem.mem_alloc(maxAlloc)); 
                        byteAlloc = maxAlloc + FAST_BLOCK_SIZE; 

                        if(!v[freed]) 
                            return { false, 1 }; 
                        
                        break; 
                    }

                    case 1:{
                        // nothing to free 
                        if(byteAlloc == 0) 
                            break; 
            
                        // free 
                        if(!mem.mem_free(v[freed])) 
                            return { false, 2 }; 
                        
                        freed++;  
                        byteAlloc = 0; 
                    }
                }
            }

            if(freed != mem.memFreeDone) 
                return { false, 3 }; 

            return { true, -1 }; 
        }

        /////////////////////////////////////////////////////////////
        pair<bool, int> max_alloc_and_split() {
            MemAllocator mem = get_alloc_instance(); 

            static const size_t maxAlloc = Data::MEM_SIZE - FAST_BLOCK_SIZE; 
            static const int maxSplits = floor(Data::MEM_SIZE / (FAST_BLOCK_SIZE + mem.MIN_USER_MEMORY)) - 1; // -1 because the maxAlloc block itself needs MIN_USER_MEMORY too
            
            // create max Block 
            char *maxBlock = (char*)mem.mem_alloc(maxAlloc); 
                
            if(!maxBlock) 
                return { false, 0 }; 

            const uintptr_t blockBegin  = reinterpret_cast<uintptr_t>(maxBlock),
                            blockEnd    = blockBegin + maxAlloc;

            set<char*> s; 
            s.insert(maxBlock); 

            // make Block viable for splitting through freeing it
            if(!mem.mem_free(maxBlock)) 
                return { false, 1 }; 


            for(int i = 0; i < maxSplits * 2; i++) {
                // split Block with size MIN_BLOCK_SIZE
                char *ptr = (char*)mem.mem_alloc(mem.MIN_USER_MEMORY); 

                // check if allocater splits more even though theres no space anymore
                if(i >= maxSplits + 1) {
                    if(ptr) 
                        return { false, 2 }; 

                    continue; 
                } 

                // check for nullptr
                if(!ptr) 
                    return { false, 3 }; 
                
                // split Block has correct size? 
                pair<size_t, size_t> blockData = mem.get_block_data((void*)ptr);
                if(i != maxSplits && blockData.first != mem.MIN_USER_MEMORY) 
                    return { false, 4 };

                // check if ptr is actually in the memory range of maxBlock
                const uintptr_t ptri  = reinterpret_cast<uintptr_t>(ptr); 
                if(ptri < blockBegin || ptri >= blockEnd) 
                    return { false, 5 };

                // check if allocator gave already used address
                if(i != maxSplits && s.find(ptr) != s.end()) 
                    return { false, 6 }; 

                s.insert(ptr); 
            }

            return { true, -1 };
        }


        pair<bool, int> basic_allignment() {
            MemAllocator mem = get_alloc_instance(); 
            int sizes[] = { 16, 32, 48, 64, 128 }; 

            for(int i = 0; i < Data::MEM_SIZE / (FAST_BLOCK_SIZE + 128) * 0.9; i++) {
                int r = ran(0, 4); 

                void *ptr = mem.mem_alloc(sizes[r]); 
                        
                if(!ptr) 
                    return { false, 0 };
                
                uintptr_t ptri  = reinterpret_cast<uintptr_t>(ptr); 
                
                if(ptri % 16 != 0) 
                    return { false, sizes[r] };

            }

            return { true, -1 };
        }
            
        pair<bool, int> split_allignment() {
            MemAllocator mem = get_alloc_instance(); 
            
            static const size_t maxAlloc = Data::MEM_SIZE - FAST_BLOCK_SIZE; 
            static const int maxSplits = floor(maxAlloc / (FAST_BLOCK_SIZE + 64)) - 1; 

            const uint8_t sizes[] = { 1, 3, 16, 28, 32, 41, 61 };

            void *p = mem.mem_alloc(maxAlloc); 
            
            if(!p) 
                return { false, 0 };

            if(!mem.mem_free(p)) 
                return { false, 1 };

            for(int i = 0; i < maxSplits; i++) {
                // split Block with size MIN_BLOCK_SIZE
                uint8_t r = ran(0, 6); 
                void *ptr = (char*)mem.mem_alloc(sizes[r]); 
                uintptr_t ptri  = reinterpret_cast<uintptr_t>(ptr); 

                if(ptri % 16 != 0) 
                    return { false, sizes[r] };
            } 

            return { true, -1 };
        }


        pair<bool, int> test() {
            MemAllocator mem = get_alloc_instance(); 

            vector<void*> v; 
            
            for(int i = 9; i >= 0; i--) {
                v.push_back(mem.mem_alloc(20 + i)); 
            } 

            for(void *p : v) 
                mem.mem_free(p); 

            mem.print_size_classes(); 
            
           // for(int i = 0; i < 10; i++) {
           //     mem.mem_alloc(20 + i); 
           //     mem.print_size_classes(); 
           // }

            mem.mem_alloc(20); 
            mem.print_size_classes(); 
            return { true, -1 }; 
        }


        ///TODO: 
        /*
            - solve remove_block_from_class segfault problem X
            - make a good data system for FAST_BLOCK_SIZE and so on X
            - complete max_alloc_and_free() X
            - max_alloc_and_split() X
            - allocator allignment fixen X
            - tests fix allignment chanegs X
            - memory allignment test X
            - size_control bitwise ops X
            - make first fit not only search one sizeClass X
            

            - cleanup code and think through logic again 
            - coalescing...
            - better output system? 
            - add seeds to randomizer
            - performance benchmarks
        */

    public: 
        AllocAndFree() {
            // init vars
            MemAllocator mem; 

            FAST_BLOCK_SIZE = mem.fast_block_size(); 
            CHAR_TEST_AMNT = Data::MEM_SIZE / (FAST_BLOCK_SIZE + mem.MIN_USER_MEMORY) * 0.9; 
            STRING_TEST_AMNT = Data::MEM_SIZE / (FAST_BLOCK_SIZE + sizeof(string)) * 0.9; 

        }; 
}; 






