### MAP BUFFER

Move to align at each write.

Buffer is a std::map< std::pair< uint64_t, uint64_t >, std::string >. Use an extravarible to store the smallest(first) start index in the map.

### VECTOR BUFFER (CURRENT)

Speed test result: 3-4 Gbps

Use vector as the underlying container of the buffer. Every byte has a 
validness flag, and bytes buffered in reassembler is the count of flags, 
which is tracked in another variable `valid_count_`.

In a `insert` call, guard code expelling data that is wholly unacceptable 
or received and cutoff partly unacceptable or received parts, then the data 
is pushed into buffer. After pushing, if the first validness flag in buffer, 
which is, that of the expected byte, push construct the longest string 
starting from the expected byte, and then push it into the stream.

After each push operation, move the contents in buffer so that the expected 
byte is in the 0-indexed position. Note that validness flags of the 
unoverwritten part of the buffer should be manually cleared to maintain 
semantic correctness (no functional impacts though).

Store the past-last-byte index in a varible `past_fianl_index_` initialized 
with max value of the type. Overwrite it when `is_last_substring` flag is true.
Close the underlying `ByteStream` when `expected_ == past_final_index_`. Note 
that this check has to be done twice in a single `insert()` call, because 
there might be 0-length data carrying the flag, which does not affect 
`expected_`.

#### SHORTCOMINGS

Low efficiency: the buffer is moved every time push operation executed. Plus 
current version of code doesn't calculate the end of copying range when 
moving, which makes it copy pointless bytes.