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

        void modifyOrder(OrderID id, Order order) {
            cancelOrder(id);
            addOrder(order);
        }

        void match(Order& incomingOrder) {

        }

        Price bestBid() const {
            if (bids.empty()) {

            } else {
                return bids.begin()->first;
            }
        }

        Price bestAsk() const {
            if (asks.empty()) {

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