#include "engine.hpp"
#include <cassert>
#include <iostream>

int main()
{
	// Orderbook ob;
	// uint32_t matches = match_order(ob, { 1, 91, 5, Side::BUY});
	// matches = match_order(ob, { 2, 93, 5, Side::BUY });
	// matches = match_order(ob, { 3, 92, 5, Side::BUY });
	// matches = match_order(ob, { 4, 90, 5, Side::BUY });
	// matches = match_order(ob, { 5, 94, 5, Side::BUY });
	// matches = match_order(ob, { 6, 96, 5, Side::BUY });
	// matches = match_order(ob, { 7, 96, 5, Side::SELL });

	Orderbook ob;
	// Insert two sell orders at different prices.
	Order sellOrder1{3, 90, 5, Side::SELL};
	Order sellOrder2{4, 95, 5, Side::SELL};
	uint32_t matches = match_order(ob, sellOrder1);
	assert(matches == 0);
	matches = match_order(ob, sellOrder2);
	assert(matches == 0);
  
	// A buy order that can match both.
	Order buyOrder{5, 100, 8, Side::BUY};
	matches = match_order(ob, buyOrder);
	assert(matches == 2);
  
	// sellOrder1 should be fully matched; sellOrder2 partially matched (remaining
	// quantity = 2).
	assert(order_exists(ob, 4));
	Order order_lookup = lookup_order_by_id(ob, 4);
	assert(order_lookup.quantity == 2);
  
	// Modify remaining order partially.
	modify_order_by_id(ob, 4, 1);
	assert(order_exists(ob, 4));
	order_lookup = lookup_order_by_id(ob, 4);
	assert(order_lookup.quantity == 1);
  
	// Fully modify the order.
	modify_order_by_id(ob, 4, 0);
	assert(!order_exists(ob, 4));
}