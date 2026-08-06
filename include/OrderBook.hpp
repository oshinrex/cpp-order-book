#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <unordered_map>

#include "Order.hpp"
#include "PriceLevel.hpp"

using Price = int64_t;
using OrderID = uint64_t;

struct OrderRef {
    std::list<Order>::iterator it;
    bool isBuy;
    Price price;
};

class OrderBook {
public:
    // Inserts a new resting order into the order book
    void addOrder(const Order& order);

    // Removes an existing order by ID
    void cancelOrder(OrderID id);

    // Updates the price and/or quantity of an existing order
    void modifyOrder(OrderID id, Price price, uint32_t quantity);
    
    // Matches an incoming order against the opposite side of the book
    void match(Order& incomingOrder);

    // Returns the highest bid price
    Price bestBid() const;

    // Returns the lowest ask price 
    Price bestAsk() const;

    // Returns true if the book contains no orders
    bool empty() const;

    // Returns the number of active orders
    size_t size() const;

private:
    // Bid book ordered from highest to lowest price
    std::map<Price, PriceLevel, std::greater<Price>> bids;

    // Ask book ordered from lowest to highest price
    std::map<Price, PriceLevel> asks;

    // Maps order IDs to their location for O(1) cancel/modify
    std::unordered_map<OrderID, OrderRef> orderIndex;
};