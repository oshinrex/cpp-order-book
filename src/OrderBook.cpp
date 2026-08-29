#include "../include/OrderBook.hpp"
#include "../include/Trade.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>

bool OrderBook::addOrder(const Order& order) {
    auto it = orderIndex.find(order.id);
    if (it == orderIndex.end()) {
    
        if (order.side == Side::buy) {
            auto& level = bids[order.price];
            level.price = order.price;

            level.orders.push_back(order);
            auto it = std::prev(level.orders.end());

            orderIndex[order.id] = OrderRef{it, true, order.price};

        } else {
            auto& level = asks[order.price];
            level.price = order.price;

            level.orders.push_back(order);
            auto it = std::prev(level.orders.end());

            orderIndex[order.id] = OrderRef{it, false, order.price};
        }
        return true;
    }
    return false;
}

void OrderBook::cancelOrder(OrderID id) {
    auto it = orderIndex.find(id);

    if (it == orderIndex.end()) {
        std::cerr << "This order was never placed\n";
        return;
    }
    
    OrderRef& o = it->second;

    if (o.isBuy) {
        bids[o.price].orders.erase(o.it);
        if (bids[o.price].orders.empty()) {
            bids.erase(o.price);
        }
    } else {
        asks[o.price].orders.erase(o.it);
        if (asks[o.price].orders.empty()) {
            asks.erase(o.price);
        }
    }

    orderIndex.erase(id);
}

void OrderBook::modifyOrder(OrderID id, Price price, uint32_t quantity) {
    auto it = orderIndex.find(id);

    if (it == orderIndex.end()) {
        return;
    }

    Order updated = *(it -> second.it);

    if (updated.price == price) {
        if (quantity <= updated.quantity) {
            it->second.it->quantity = quantity;
            return;
        }
        updated.quantity = quantity;
        cancelOrder(id);
        addOrder(updated);
        return;
    }

    updated.price = price;
    updated.quantity = quantity;

    cancelOrder(id);
    addOrder(updated);
}

std::vector<Trade> OrderBook::match(Order& incomingOrder) {
    std::vector<Trade> trades;
    const uint64_t timestamp = incomingOrder.timestamp;

    if (incomingOrder.side == Side::buy) {
        while (incomingOrder.quantity > 0 && !asks.empty() && (incomingOrder.order_type == OrderType::market || asks.begin()->first <= incomingOrder.price)) {
            auto& bestLevel = asks.begin()->second;
            auto restingIt = bestLevel.orders.begin();
            Order& restingOrder = *restingIt;

            uint32_t traded = std::min(incomingOrder.quantity, restingOrder.quantity);
            incomingOrder.quantity -= traded;
            restingOrder.quantity -= traded;

            // Capture what we need before any erase invalidates restingOrder.
            OrderID restingId = restingOrder.id;
            Price restingPrice = restingOrder.price;
        
            if (restingOrder.quantity == 0) {
                orderIndex.erase(restingId);
                bestLevel.orders.erase(restingIt);
            }

            if (bestLevel.orders.empty()) {
                asks.erase(asks.begin());
            }

            trades.push_back(Trade{incomingOrder.id, restingId, restingPrice, traded, timestamp});
        }

        if (incomingOrder.quantity != 0 && incomingOrder.order_type == OrderType::limit) {
            addOrder(incomingOrder);
        }
        return trades;

    } else {
        while (incomingOrder.quantity > 0 && !bids.empty() && (incomingOrder.order_type == OrderType::market || bids.begin()->first >= incomingOrder.price)) {
            auto& bestLevel = bids.begin()->second;
            auto restingIt = bestLevel.orders.begin();
            Order& restingOrder = *restingIt;

            uint32_t traded = std::min(incomingOrder.quantity, restingOrder.quantity);
            incomingOrder.quantity -= traded;
            restingOrder.quantity -= traded;

            OrderID restingId = restingOrder.id;
            Price restingPrice = restingOrder.price;
            
            if (restingOrder.quantity == 0) {
                orderIndex.erase(restingId);
                bestLevel.orders.erase(restingIt);
            }

            if (bestLevel.orders.empty()) {
                bids.erase(bids.begin());
            }

            // incoming order is the seller here, resting order is the buyer.
            trades.push_back(Trade{restingId, incomingOrder.id, restingPrice, traded, timestamp});
        }

        if (incomingOrder.quantity != 0 && incomingOrder.order_type == OrderType::limit) {
            addOrder(incomingOrder);
        }
        return trades;
    }
}

Price OrderBook::bestBid() const {
    return bids.empty() ? 0 : bids.begin()->first;
}

Price OrderBook::bestAsk() const {
    return asks.empty() ? 0 : asks.begin() -> first;
}

bool OrderBook::empty() const {
    return orderIndex.empty();
}

size_t OrderBook::size() const {
    return orderIndex.size();
}

void OrderBook::printBook() const {
    std::cout << "Bids:\n";
    for (const auto& [price, level] : bids) {
        std::cout << "Price: " << price << ", Quantity: ";
        uint32_t totalQuantity = 0;
        for (const auto& order : level.orders) {
            totalQuantity += order.quantity;
        }
        std::cout << totalQuantity << "\n";
    }

    std::cout << "Asks:\n";
    for (const auto& [price, level] : asks) {
        std::cout << "Price: " << price << ", Quantity: ";
        uint32_t totalQuantity = 0;
        for (const auto& order : level.orders) {
            totalQuantity += order.quantity;    
        }
        std::cout << totalQuantity << "\n";
    }
}