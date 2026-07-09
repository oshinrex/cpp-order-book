#include <iostream>
#include <cstdint>
#include <map>
#include <functional>
#include <list>
#include <unordered_map>
#include "PriceLevel.hpp"
#include "Order.hpp"

using Price = int64_t;
using OrderID = uint64_t;

struct OrderRef {
    std::list<Order>::iterator it;
    bool isBuy;
    Price price;
};

class OrderBook {
    private:
        std::map<Price, PriceLevel, std::greater<Price>> bids;
        std::map<Price, PriceLevel> asks;
        std::unordered_map<OrderID, OrderRef> orderIndex;

    public:
        void addOrder(const Order& order) {
            if (orderIndex.find(order.id) != orderIndex.end()) {
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

        void cancelOrder(OrderID id) {
            if (orderIndex.find(id) == orderIndex.end()) {
                std::cerr << "This order was never placed\n";
                return;
            }
            
            auto it = orderIndex.find(id);
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

        void modifyOrder(OrderID id, Price price, uint32_t quantity) {
            auto it = orderIndex.find(id);

            if (it == orderIndex.end()) {
                return;
            }

            Order& order = *(it -> second.it);

            if (order.price == price) {
                order.quantity = quantity;
            } else {
                Order updated = order; 
                updated.price = price;
                updated.quantity = quantity; 

                cancelOrder(id);
                addOrder(updated);
            }
        }

        void match(Order& incomingOrder) {
            if (incomingOrder.side == Side::buy) {
                while (incomingOrder.quantity > 0 && !asks.empty() && asks.begin()->first <= incomingOrder.price) {
                    auto& bestLevel = asks.begin()->second;
                    auto restingIt = bestLevel.orders.begin();
                    Order& restingOrder = *restingIt;

                    int32_t traded = std::min(incomingOrder.quantity, restingOrder.quantity);
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
                while (incomingOrder.price > 0 && !bids.empty() && bids.begin()->first >= incomingOrder.price) {
                    auto& bestLevel = bids.begin()->second;
                    auto restingIt = bestLevel.orders.begin();
                    Order& restingOrder = *restingIt;
                    
                    int32_t traded = std::min(incomingOrder.quantity, restingOrder.quantity);
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

        Price bestBid() const {
            if (bids.empty()) {
                return 0;
            } else {
                return bids.begin()->first;
            }
        }

        Price bestAsk() const {
            if (asks.empty()) {
                return 0;
            } else {
                return asks.begin()->first;
            }
        }

        bool empty() const {
            return orderIndex.empty();
        }

        int size() const {
            return orderIndex.size();
        }
};