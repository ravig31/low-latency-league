#include <iostream>
#include "engine.hpp"
#include <cassert>

int main(){
    std::cout << "Test 1: Simple match and modify" << std::endl;
    Orderbook ob;
    // Insert a sell order.
    Order sellOrder{1, 100, 10, Side::SELL};
    uint32_t matches = match_order(ob, sellOrder);
  
    // A buy order that partially matches the sell order.
    Order buyOrder{2, 100, 5, Side::BUY};
    matches = match_order(ob, buyOrder);
  
    // Remaining sell order should have quantity 5.
    assert(order_exists(ob, 1));
    Order order_lookup = lookup_order_by_id(ob, 1);
  
    // Modify the remaining sell order.
    modify_order_by_id(ob, 1, 0);
  
    std::cout << "Test 1 passed." << std::endl;
}