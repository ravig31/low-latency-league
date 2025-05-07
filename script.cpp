#include "engine.hpp"
#include <cassert>

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
	Order sellOrder1{101, 100, 5, Side::SELL};
	Order sellOrder2{102, 100, 7, Side::SELL};
	match_order(ob, sellOrder1);
	match_order(ob, sellOrder2);
	uint32_t volume_sell = get_volume_at_level(ob, Side::SELL, 100);
	assert(volume_sell == 12); // 5 + 7 = 12
}