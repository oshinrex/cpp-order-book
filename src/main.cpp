#include <iostream>
#include "../include/OrderBook.hpp"

int main() {
    OrderBook book; 

    Order buy1{1, Side::buy, OrderType::limit, 50, 100, 0};
    Order sell1{2, Side::sell, OrderType::limit, 51, 60, 0};
    Order sell2{3, Side::sell, OrderType::limit, 49, 80, 0};

    book.match(buy1);
    book.match(sell1);
    std::cout << "book after resting orders";
    book.printBook();

    auto trades = book.match(sell2);
    std::cout << "\nTrades from crossing order";
    for (const auto& t : trades) {
        std::cout << "buy=" << t.buyOrderId << " sell=" << t.sellOrderId << " price=" << t.price << " qty=" << t.quantity << "\n";
    };

    std::cout << "book after trade";
    book.printBook();
}