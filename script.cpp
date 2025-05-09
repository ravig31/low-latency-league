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
	Order buyOrder1{103, 100, 10, Side::BUY};
	Order buyOrder2{104, 101, 5, Side::BUY};
	match_order(ob, buyOrder1);
	match_order(ob, buyOrder2);
	uint32_t volume_buy_100 = get_volume_at_level(ob, Side::BUY, 100);
	uint32_t volume_buy_101 = get_volume_at_level(ob, Side::BUY, 101);
	assert(volume_buy_100 == 10);
	assert(volume_buy_101 == 5);
}