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

        /*static  inline */ size_t     FAST_BLOCK_SIZE; 
        /*static  inline*/  int        CHAR_TEST_AMNT; 
        /*static  inline*/  int        STRING_TEST_AMNT;

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
            
            char *curr = (char*)mem.mem_alloc(sizeof(char)); // this address should be the only we get in this whole test
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
            size_t byteAlloc = 0; 

            char *a = (char*)mem.mem_alloc(Data::MEM_SIZE - FAST_BLOCK_SIZE); 
            if(!a)
                return { false, 0 };

            byteAlloc = Data::MEM_SIZE; 
            if(byteAlloc != mem.offset)
                return { false, 1 };
            
            if(mem.mem_alloc(1))
                return { false, 2 };

            if(!mem.mem_free(a) && mem.offset > 0)
                return { false, 3 };
                       
            
            return { true, -1 };
        }


        ///TODO: 
        /* 
            - more tests (max_alloc_and_free, boundary checks, and more i cant think of rn) 
            - find a solution for the stupid static vars like FAST_BLOCK_SIZE 
        */

    public: 
        AllocAndFree() {
            // init static vars
            MemAllocator mem; 

            FAST_BLOCK_SIZE = mem.fast_block_size();  
            CHAR_TEST_AMNT = Data::MEM_SIZE / (FAST_BLOCK_SIZE + mem.MIN_BLOCK_SIZE) * 0.9; 
            STRING_TEST_AMNT = Data::MEM_SIZE / (FAST_BLOCK_SIZE + sizeof(string)) * 0.9; 
        }; 
}; 