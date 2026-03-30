#include <iostream> 
#include "allocAndFree.cpp"

using namespace std; 

class Tests {
    private: 
        AllocAndFree aaf; 

        inline void output(pair<bool, int> p) const {
            if(!p.first || p.second > -1) {
                cout << p.first << " - " << p.second << endl; 
                return; 
            } 

            cout << p.first << endl; 
        }

    public: 
        void run_tests() {
            // output(aaf.pure_alloc());
            // output(aaf.alloc_and_free());
            // output(aaf.trade_alloc_and_free());
            // output(aaf.invalid_ptr_free());
            // output(aaf.random_type_alloc());
            output(aaf.max_alloc_and_free()); 
        }

}; 