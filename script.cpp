#include "engine.hpp"
#include <cassert>

int main()
{
	Orderbook ob;
	uint32_t matches = match_order(ob, { 1, 91, 5, Side::BUY});
	matches = match_order(ob, { 2, 93, 5, Side::BUY });
	matches = match_order(ob, { 3, 92, 5, Side::BUY });
	matches = match_order(ob, { 4, 90, 5, Side::BUY });
	matches = match_order(ob, { 3, 94, 5, Side::BUY });
	matches = match_order(ob, { 4, 100, 5, Side::BUY });
	// Orderbook ob;
	// // Insert two sell orders at the same price.
	// Order sellOrder1{32, 100, 4, Side::SELL};
	// Order sellOrder2{33, 100, 6, Side::SELL};
	// match_order(ob, sellOrder1);
	// match_order(ob, sellOrder2);
  
	// // Insert a buy order that partially fills both sell orders.
	// Order buyOrder{34, 100, 8, Side::BUY};
	// uint32_t matches = match_order(ob, buyOrder);
	// assert(matches == 2);
  
	// // sellOrder1 fully matched; sellOrder2 partially filled (remaining = 2).
	// assert(!order_exists(ob, 32));
	// assert(order_exists(ob, 33));
	// Order order_lookup = lookup_order_by_id(ob, 33);
	// assert(order_lookup.quantity == 2);
}