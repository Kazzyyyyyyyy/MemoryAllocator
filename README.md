# MemoryAllocator
C++ mmap memory allocator

# Implemented Modes
 - PRECISE 
 - FAST

# State of the project 
Same as with my (LockFreeQueue)[https://github.com/Kazzyyyyyyyy/LockFreeQueue] I greatly overestimated my expertise when I first started this project. Now nearly a year later I came back to the project and found out that its in a horrible state. </br>  
Currently reworking pretty much everything. 

# Benchmarks (for the original version, the rework has no Bench's yet) 
FAST: ~25ns (median out of 10k allocs) </br> 
PRECISE: ~70ns (median out of 10k allocs) </br> 