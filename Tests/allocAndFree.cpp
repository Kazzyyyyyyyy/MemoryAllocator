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

using namespace std; 

class AllocAndFree {
    private: 
        friend class Tests;

        size_t     FAST_BLOCK_SIZE; 
        int        CHAR_TEST_AMNT; 
        int        STRING_TEST_AMNT;

        inline size_t ran(size_t min = 0, size_t max = SIZE_MAX) const {
            static random_device rd;
            static mt19937 gen(rd());
            uniform_int_distribution<> dist(min, max);
            return dist(gen);
        }

        inline MemAllocator<FAST, Data::MEM_SIZE> get_alloc_instance() const {
            return MemAllocator<FAST, Data::MEM_SIZE>();
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
            if(mem.memAlloc != CHAR_TEST_AMNT) 
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
               
            if(mem.memFree != CHAR_TEST_AMNT)
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
            if(mem.memFree != CHAR_TEST_AMNT || mem.memAlloc != CHAR_TEST_AMNT + 1)
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

            if(mem.memFree > 0) 
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
                        byteAlloc += FAST_BLOCK_SIZE + mem.MIN_BLOCK_SIZE; // char < MIN_BLOCK_SIZE
                        break; 

                    case 1: 
                        mem.mem_alloc(sizeof(int));
                        byteAlloc += FAST_BLOCK_SIZE + sizeof(int); 
                        break;
                    
                    case 2: 
                        mem.mem_alloc(sizeof(size_t));
                        byteAlloc += FAST_BLOCK_SIZE + sizeof(size_t); 
                        break;
                    
                    case 3: 
                        mem.mem_alloc(24); 
                        byteAlloc += FAST_BLOCK_SIZE + 24; 
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

            if(freed != mem.memFree) 
                return { false, 3 }; 

            return { true, -1 }; 
        }

        //pair<bool, int> max_alloc_and_split() {}
        

        //void read_data() {
        //    cout << FAST_BLOCK_SIZE << endl; 
        //    cout << CHAR_TEST_AMNT << endl; 
        //    cout << STRING_TEST_AMNT << endl; 
        //}
        
        //void remove_block_from_class() {
        //    MemAllocator mem = get_alloc_instance();
        //    vector<int*> v; 

        //    for(int i = 10; i >= 0; i--) 
        //        v.push_back((int*)mem.mem_alloc(i + 100));
        //        
        //    mem.print_size_classes(); 

        //    for(int* a : v) 
        //        mem.mem_free(a); 

        //    mem.print_size_classes(); 
        //

        //    int *a = (int*)mem.mem_alloc(101);
        //    int *b = (int*)mem.mem_alloc(104);
        //    int *c = (int*)mem.mem_alloc(109);

        //    mem.print_size_classes(); 
        //    
        //}

        ///TODO: 
        /*
            - solve remove_block_from_class segfault problem X
            - make a good data system for FAST_BLOCK_SIZE and so on 
            - complete max_alloc_and_free()

            - max_alloc_and_split... 
            - add seeds to randomizer
            - cleanup code 
        */

    public: 
        AllocAndFree() {
            // init vars
            MemAllocator mem; 

            FAST_BLOCK_SIZE = mem.fast_block_size(); 
            CHAR_TEST_AMNT = Data::MEM_SIZE / (FAST_BLOCK_SIZE + mem.MIN_BLOCK_SIZE) * 0.9; 
            STRING_TEST_AMNT = Data::MEM_SIZE / (FAST_BLOCK_SIZE + sizeof(string)) * 0.9; 
        }; 
}; 






