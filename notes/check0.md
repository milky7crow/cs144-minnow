### STRAIGHT BUFFER (CURRENT)

Speed test result: 14~21 Gbps

Mainain a head and a tail index, push at tail and pop at head. Move data back 
to the start when there's no enough space to hold the new data slice.

Unknown behavior: When accidentally swapped two args of the constructor of 
string (`byte_stream.cc:6`), overwrite test broked. The strange behavior is, 
the content of `buffer_` changed between adjacent `peek()` calls.


### CIRCULAR BUFFER

Push and pop bytes. Reassemble string when peeking.

TBE: What's the / Is there performance difference between byte-wise, 
index-based copy and iterator-based copy? In iterator-based copy, two copy 
operations are performed if actual data is not contiguous in memory.
