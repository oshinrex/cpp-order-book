#pragma once

#include <vector>
#include "Order.hpp"

// A node in an intrusive doubly-linked list of Order, plus the pool that
// hands them out. "Intrusive" means the prev/next pointers live inside the
// node itself instead of a separate container managing them - this is what
// lets us preallocate every node once up front instead of calling new/delete
// per order.
struct OrderNode {
    Order data;
    OrderNode* prev = nullptr;
    OrderNode* next = nullptr;
};

class OrderNodePool {
public:
    explicit OrderNodePool(size_t capacity) : storage_(capacity) {
        freeList_.reserve(capacity);
        // Push in reverse so acquire() hands out storage_[0] first - not
        // required for correctness, just makes early behavior easier to
        // reason about if you print addresses while debugging.
        for (size_t i = capacity; i-- > 0;) {
            freeList_.push_back(&storage_[i]);
        }
    }

    OrderNode* acquire() {
        if (freeList_.empty()) {
            // Pool exhausted: fall back to a real heap allocation rather
            // than crash. Rare in practice if capacity is sized generously,
            // but must be handled - this node must still be release()-able
            // as if it came from the pool.
            return new OrderNode();
        }
        OrderNode* node = freeList_.back();
        freeList_.pop_back();
        return node;
    }

    void release(OrderNode* node) {
        node->prev = nullptr;
        node->next = nullptr;
        if (node >= storage_.data() && node < storage_.data() + storage_.size()) {
            freeList_.push_back(node);
        } else {
            delete node; // was an overflow allocation
        }
    }

private:
    std::vector<OrderNode> storage_;
    std::vector<OrderNode*> freeList_;
};
