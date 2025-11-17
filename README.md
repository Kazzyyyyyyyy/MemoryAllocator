# MemoryAllocator
C++ mmap memory allocator

# What I've learned 
 - working with raw memory 
 - how an (Block) allocator works 
 - the difference between allocators for different use cases (speed, safety, precision) 
 - coalescing, splitting, best/first_fit
 - how to better test low lvl applications (pretty hard)
 - how to use MMAP through/and WSL (on windows)
 - new clean code principles (and my comments are also pretty valid I think)

# Implemented Modes
 - PRECISE 
 - FAST

# State of the project 
For me the Project is done. I learned all I wanted to learn. Originally I planed to make a SAFE mode too, but whats the point? 
I now know how to work with memory and I'm pretty sure I'm able to use if and return statements correctly. I already had a plan on how to make the SAFE mode and what to look for (double FREE, external pointer free, etc), but I think 
it would just be wasted time and I rather should put that time into a new project.

# Notes 
Both allocators only got one safety check, that beeing if the requested size is smaller than MIN_BLOCK_SIZE(4), so be careful if (whyever) you are gonna use them.

FAST: ~25ns (median out of 10k allocs)
PRECISE: ~70ns (median out of 10k allocs)