#pragma once
#include <iostream>
#include <list>
#include "Order.hpp"
#include "Types.hpp"

struct PriceLevel {
    std::list<Order> orders; 
    Price price;
};

