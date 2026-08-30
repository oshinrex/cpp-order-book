#include "../include/OrderBook.hpp"
#include "../include/Trade.hpp"
#include <iostream>
#include <algorithm>

bool OrderBook::addOrder(const Order& order) {
    auto it = orderIndex.find(order.id);
    if (it == orderIndex.end()) {

        if (order.side == Side::buy) {
            auto& level = bids[order.price];
            level.price = order.price;

            OrderNode* node = nodePool_.acquire();
            node->data = order;
            node->prev = level.tail;
            node->next = nullptr;

            if (level.tail == nullptr) {
                level.head = node;
            } else {
                level.tail->next = node;
            }
            level.tail = node;

            orderIndex[order.id] = OrderRef{node, true, order.price};

        } else {
            auto& level = asks[order.price];
            level.price = order.price;

            OrderNode* node = nodePool_.acquire();
            node->data = order;
            node->prev = level.tail;
            node->next = nullptr;

            if (level.tail == nullptr) {
                level.head = node;
            } else {
                level.tail->next = node;
            }
            level.tail = node;

            orderIndex[order.id] = OrderRef{node, false, order.price};
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
    OrderNode* node = o.node;

    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;

    if (o.isBuy) {
        PriceLevel& level = bids[o.price];
        if (level.head == node) level.head = node->next;
        if (level.tail == node) level.tail = node->prev;

        nodePool_.release(node);

        if (level.head == nullptr) {
            bids.erase(o.price);
        }
    } else {
        PriceLevel& level = asks[o.price];
        if (level.head == node) level.head = node->next;
        if (level.tail == node) level.tail = node->prev;

        nodePool_.release(node);

        if (level.head == nullptr) {
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

    Order updated = it->second.node->data;

    if (updated.price == price) {
        if (quantity <= updated.quantity) {
            it->second.node->data.quantity = quantity;
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
        while (incomingOrder.quantity > 0 && !asks.empty() &&
       (incomingOrder.order_type == OrderType::market || asks.begin()->first <= incomingOrder.price)) {
            auto& bestLevel = asks.begin()->second;
            OrderNode* restingNode = bestLevel.head;
            Order& restingOrder = restingNode->data;

            uint32_t traded = std::min(incomingOrder.quantity, restingOrder.quantity);
            incomingOrder.quantity -= traded;
            restingOrder.quantity -= traded;

            // Capture what we need before any release invalidates restingOrder.
            OrderID restingId = restingOrder.id;
            Price restingPrice = restingOrder.price;

            if (restingOrder.quantity == 0) {
                orderIndex.erase(restingId);

                bestLevel.head = restingNode->next;
                if (bestLevel.head) {
                    bestLevel.head->prev = nullptr;
                } else {
                    bestLevel.tail = nullptr;
                }
                nodePool_.release(restingNode);
            }

            if (bestLevel.head == nullptr) {
                asks.erase(asks.begin());
            }

            trades.push_back(Trade{incomingOrder.id, restingId, restingPrice, traded, timestamp});
        }

        if (incomingOrder.quantity != 0 && incomingOrder.order_type == OrderType::limit) {
            addOrder(incomingOrder);
        }
        return trades;

    } else {
        while (incomingOrder.quantity > 0 && !bids.empty() &&
       (incomingOrder.order_type == OrderType::market || bids.begin()->first >= incomingOrder.price)) {
            auto& bestLevel = bids.begin()->second;
            OrderNode* restingNode = bestLevel.head;
            Order& restingOrder = restingNode->data;

            uint32_t traded = std::min(incomingOrder.quantity, restingOrder.quantity);
            incomingOrder.quantity -= traded;
            restingOrder.quantity -= traded;

            OrderID restingId = restingOrder.id;
            Price restingPrice = restingOrder.price;

            if (restingOrder.quantity == 0) {
                orderIndex.erase(restingId);

                bestLevel.head = restingNode->next;
                if (bestLevel.head) {
                    bestLevel.head->prev = nullptr;
                } else {
                    bestLevel.tail = nullptr;
                }
                nodePool_.release(restingNode);
            }

            if (bestLevel.head == nullptr) {
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
        for (OrderNode* n = level.head; n != nullptr; n = n->next) {
            totalQuantity += n->data.quantity;
        }
        std::cout << totalQuantity << "\n";
    }

    std::cout << "Asks:\n";
    for (const auto& [price, level] : asks) {
        std::cout << "Price: " << price << ", Quantity: ";
        uint32_t totalQuantity = 0;
        for (OrderNode* n = level.head; n != nullptr; n = n->next) {
            totalQuantity += n->data.quantity;
        }
        std::cout << totalQuantity << "\n";
    }
}
