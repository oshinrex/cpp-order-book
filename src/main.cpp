#include <iostream>
#include <cassert>
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

    OrderBook fifo; 
    Order a{10, Side::buy, OrderType::limit, 100, 50, 0};
    Order b{11, Side::buy, OrderType::limit, 100, 50, 0}; 

    fifo.match(a);
    fifo.match(b);

    Order aggressor{12, Side::sell, OrderType::limit, 100, 60, 0};
    auto fifotrades = fifo.match(aggressor);

    assert(fifotrades.size() == 2);
    assert(fifotrades[0].buyOrderId == 10);
    assert(fifotrades[1].buyOrderId == 11);
    std::cout << "FIFO priority: OK\n";

    // test bestBid/bestAsk
    OrderBook book2;
    assert(book2.bestBid() == 0);
    assert(book2.bestAsk() == 0);

    Order buyLow{20, Side::buy, OrderType::limit, 90, 10, 0};
    Order buyHigh{21, Side::buy, OrderType::limit, 95, 10, 0};
    book2.match(buyLow);
    book2.match(buyHigh);
    assert(book2.bestBid() == 95); 

    Order sellHigh{22, Side::sell, OrderType::limit, 110, 10, 0};
    Order sellLow{23, Side::sell, OrderType::limit, 105, 10, 0};
    book2.match(sellHigh);
    book2.match(sellLow);
    assert(book2.bestAsk() == 105);

    book2.cancelOrder(21); // remove the best bid (95)
    assert(book2.bestBid() == 90); // must fall back to the next level, not break
    std::cout << "bestBid/bestAsk: OK\n";

    // test cancelOrder: order and empty price level both actually removed
    {
        OrderBook cancelBook;
        Order c{30, Side::buy, OrderType::limit, 75, 20, 0};
        cancelBook.match(c);
        assert(cancelBook.size() == 1);
        assert(cancelBook.bestBid() == 75);

        cancelBook.cancelOrder(30);
        assert(cancelBook.size() == 0);
        assert(cancelBook.empty());
        assert(cancelBook.bestBid() == 0); // price level itself must be erased, not left empty

        std::cout << "cancelOrder: OK\n";
    }

    // test modifyOrder, in-place (qty decrease) vs requeue (qty increase)
    {
        OrderBook modBook;
        Order x{40, Side::buy, OrderType::limit, 100, 50, 0}; // added first
        Order y{41, Side::buy, OrderType::limit, 100, 50, 0}; // added second
        modBook.match(x);
        modBook.match(y);

        modBook.modifyOrder(40, 100, 20); // same price, smaller qty -> stays in place

        Order aggressor1{42, Side::sell, OrderType::limit, 100, 30, 0};
        auto t1 = modBook.match(aggressor1);
        assert(t1.size() == 2);
        assert(t1[0].buyOrderId == 40); // x still fills first
        assert(t1[1].buyOrderId == 41);

        Order z{43, Side::buy, OrderType::limit, 100, 10, 0};
        modBook.match(z); // rests behind y

        modBook.modifyOrder(41, 100, 60); // same price, larger qty -> requeued to the back

        Order aggressor2{44, Side::sell, OrderType::limit, 100, 10, 0};
        auto t2 = modBook.match(aggressor2);
        assert(t2.size() == 1);
        assert(t2[0].buyOrderId == 43); // z fills first now; y was sent to the back

        std::cout << "modifyOrder: OK\n";
    }

    // --- market order: empty book -> no trades, no rest ---
    {
        OrderBook mktBook;
        Order m{50, Side::buy, OrderType::market, 0, 30, 0};
        auto trades = mktBook.match(m);
        assert(trades.empty());
        assert(mktBook.empty()); // must not have rested on the book
        std::cout << "market order (empty book): OK\n";
    }

    // --- market order: partial fill, remainder discarded, not rested ---
    {
        OrderBook mktBook2;
        Order resting{51, Side::sell, OrderType::limit, 200, 10, 0};
        mktBook2.match(resting);

        Order m{52, Side::buy, OrderType::market, 0, 30, 0}; // wants 30, only 10 available
        auto trades = mktBook2.match(m);
        assert(trades.size() == 1);
        assert(trades[0].quantity == 10);
        assert(mktBook2.size() == 0); // resting order fully consumed, market order's remainder discarded, nothing left
        std::cout << "market order (partial fill): OK\n";
    }
}