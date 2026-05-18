#include <iostream> 
#include "Tests/testMain.cpp"

//#include "memAlloc.h" 

using namespace std; 

int main() {
    Tests t;
   
    t.run_tests();
    
    

    //MemAllocator mem; 


    //void *a = mem.mem_alloc(16);
    //void *b = mem.mem_alloc(32);
    //void *c = mem.mem_alloc(48);

    //if(!a) 
    //    cout << "!a" << endl;
    //
    //if(!b) 
    //    cout << "!a" << endl;
    //
    //if(!c) 
    //    cout << "!a" << endl;

    //mem.mem_free(c); 
    //mem.mem_free(a); 
    //mem.mem_free(b); 

    //mem.print_prevSize(d); 


    //size_t s = 9223372036854775807; 
    //uint64_t sx = s / 48; 
    //cout << s << " - " << sx << " - " << sizeof(uint64_t) << endl;

    //string x = "1010101010101010101010101010101010101010101010101010101010"; 

    //cout << x.size() << endl;

    return 0; 
}

// 111111
//
// 192153584101141161
