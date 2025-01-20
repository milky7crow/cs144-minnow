### VECTOR BUFFER (CURRENT)

Speed test result: 13~20 Gbps

Use std::string to store data in buffer, std::list<pair<uint64_t, uint64_t>>
to store start and end index of data segments in buffer. Segments are merged
at each change.

After copying the necessary data into the buffer, program checks if the 
starting index of the first data segment is identical to the expecting index, 
if so, then a pushing action should be taken.

In a pushing action, data in the first segment is pushed into output 
ByteStream, following data in the buffer is moved to the beginning. Expecting 
index and segment location list are updated.