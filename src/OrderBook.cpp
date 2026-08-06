#include "../include/OrderBook.hpp"

void OrderBook::addOrder(const Order& order) {
    auto it = orderIndex.find(order.id);
    if (it != orderIndex.end()) {
        std::cerr << "Order already exists\n";
        return;
    }
    
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

void OrderBook::match(Order& incomingOrder) {
    if (incomingOrder.side == Side::buy) {
        while (incomingOrder.quantity > 0 && !asks.empty() && asks.begin()->first <= incomingOrder.price) {
            auto& bestLevel = asks.begin()->second;
            auto restingIt = bestLevel.orders.begin();
            Order& restingOrder = *restingIt;

            uint32_t traded = std::min(incomingOrder.quantity, restingOrder.quantity);
            incomingOrder.quantity -= traded;
            restingOrder.quantity -= traded;

            if (restingOrder.quantity == 0) {
                orderIndex.erase(restingOrder.id);
                bestLevel.orders.erase(restingIt);
            }
            
            if (bestLevel.orders.empty()) {
                asks.erase(asks.begin());
            }
        }
    
        if (incomingOrder.quantity != 0) {
            addOrder(incomingOrder);
        }

    } else {
        while (incomingOrder.quantity > 0 && !bids.empty() && bids.begin()->first >= incomingOrder.price) {
            auto& bestLevel = bids.begin()->second;
            auto restingIt = bestLevel.orders.begin();
            Order& restingOrder = *restingIt;
            
            uint32_t traded = std::min(incomingOrder.quantity, restingOrder.quantity);
            incomingOrder.quantity -= traded;
            restingOrder.quantity -= traded;
            
            if (restingOrder.quantity == 0) {
                orderIndex.erase(restingOrder.id);
                bestLevel.orders.erase(restingIt);
            }

            if (bestLevel.orders.empty()) {
                bids.erase(bids.begin());
            }
        }
        
        if (incomingOrder.quantity != 0) {
            addOrder(incomingOrder);
        }
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