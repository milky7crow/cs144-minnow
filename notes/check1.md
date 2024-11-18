### circular buffer
...

### map buffer

Move to align at each write.

Buffer is a std::map< std::pair< uint64_t, uint64_t >, std::string >. Use an extravarible to store the smallest(first) start index in the map.

### straight buffer

Buffer is a vector with the length of capacity, each byte has a validness bool.
