#pragma once

#include <iostream>
#include "Types.hpp"

enum class Side {buy, sell};
enum class OrderType {limit, market};

struct Order {
    uint64_t id; 
    Side side;
    OrderType order_type;
    Price price;
    std::uint32_t quantity; 
    std::uint64_t timestamp; 
};