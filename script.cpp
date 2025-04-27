#include "engine.hpp"
#include <cassert>

int main()
{
	// Orderbook ob;
	// uint32_t matches = match_order(ob, { 1, 100, 5, Side::SELL });
	// matches = match_order(ob, { 2, 100, 5, Side::BUY });
	// matches = match_order(ob, { 2, 90, 5, Side::BUY });
	// matches = match_order(ob, { 2, 92, 5, Side::BUY });
	// matches = match_order(ob, { 2, 95, 5,Side::SELL});
	Orderbook ob;
	// Insert SELL orders at price levels 100 and 101.
	Order sellOrder1{ 200, 100, 10, Side::SELL };
	Order sellOrder2{ 201, 100, 20, Side::SELL };
	Order sellOrder3{ 202, 101, 15, Side::SELL };
	match_order(ob, sellOrder1);
	match_order(ob, sellOrder2);
	match_order(ob, sellOrder3);
	// Modify sellOrder1: reduce quantity to 5.
	modify_order_by_id(ob, 200, 5);
	// Check volume at level 100 for SELL: 5 + 20 = 25.
	uint32_t volume_100 = get_volume_at_level(ob, Side::SELL, 100);
	assert(volume_100 == 25);
	// Check volume at level 101 for SELL.
	uint32_t volume_101 = get_volume_at_level(ob, Side::SELL, 101);
	assert(volume_101 == 15);
}