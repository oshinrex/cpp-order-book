#include <iostream>

enum class Side {buy, sell};

struct Order {
    uint64_t id; 
    Side side;
    int64_t price;
    std::uint32_t quantity; 
    std::uint64_t timestamp; 
};