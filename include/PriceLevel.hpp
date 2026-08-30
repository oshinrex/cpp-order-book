#pragma once
#include <iostream>
#include "Order.hpp"
#include "OrderNodePool.hpp"
#include "Types.hpp"

struct PriceLevel {
    OrderNode* head = nullptr;
    OrderNode* tail = nullptr;
    Price price;
};

