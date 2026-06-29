#include <iostream>
#include <list>
#include <Order.hpp>


struct PriceLevel {
    std::list<Order> orders; 
    double price;
};

