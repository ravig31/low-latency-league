#include "engine.hpp"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>

// #define DEBUG_LOG(msg)
// 	std::cout << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " - " << msg << std::endl
// #define ERROR_LOG(msg)
// 	std::cout << "[ERROR] " << __FILE__ << ":" << __LINE__ << " - " << msg << std::endl
// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.
static std::function<bool(LevelPriceType, LevelPriceType)> get_cond(Side side)
{
	return side == Side::BUY ? [](LevelPriceType a, LevelPriceType b) { return a > b; }
							 : [](LevelPriceType a, LevelPriceType b) { return a < b; };
}

inline __attribute__((always_inline, hot)) void add_order(
	LevelPriceType price,
	IdType order_id,
	Side side,
	QuantityType quantity,
	Levels& priceLevels
) noexcept
{
	// if (order.id >= orders.size())
	// {
	// 	ERROR_LOG(
	// 		"Order ID " << order.id << " is out of bounds for orders array (size: " << orders.size()
	// 					<< ")"
	// 	);
	// 	return;
	// }
	price = side == Side::BUY ? -1 * price : price;

	if (priceLevels.empty()) [[unlikely]]
	{
		priceLevels.push_back({ price, PriceLevel(order_id, quantity) });
		return;
	}

	auto rit = priceLevels.rbegin();
	while (rit != priceLevels.rend() && price > rit->first)
	{
		++rit;
	}

	if (rit != priceLevels.rend() && rit->first == price) [[likely]]
	{
		rit->second.add_order(order_id, quantity);
	}
	else [[unlikely]]
	{
		priceLevels.insert(rit.base(), { price, PriceLevel(order_id, quantity) });
	}
}

inline __attribute__((always_inline, hot)) uint32_t process_orders(
	LevelPriceType price,
	QuantityType& quantity_ref,
	Levels& priceLevels,
	Orders& orders,
	OrdersActive& ordersActive,
	Cond cond
) noexcept
{
	uint32_t matchCount = 0;
	auto it = priceLevels.rbegin();

	while (it != priceLevels.rend() && quantity_ref > 0 &&
		   (abs(it->first) == price || cond(price, abs(it->first))))
	{
		auto& ordersAtLevel = it->second.orders;
		while (!ordersAtLevel.empty() && quantity_ref > 0)
		{
			IdType orderId = ordersAtLevel.front();
			auto& counterOrder = orders[orderId];

			QuantityType trade = std::min(quantity_ref, counterOrder.quantity);
			quantity_ref -= trade;
			counterOrder.quantity -= trade;
			it->second.volume -= trade; // Decrement total volume at level

			if (counterOrder.quantity == 0) [[likely]]
			{
				ordersAtLevel.pop_front();
				ordersActive.reset(orderId);
			}
			++matchCount;
		}

		if (ordersAtLevel.empty()) [[unlikely]]
		{
			it = decltype(it)(priceLevels.erase(std::next(it).base()));
		}
		else
		{
			++it; // Use pre-increment for clarity and efficiency
		}
	}
	return matchCount;
}

uint32_t match_order(Orderbook& orderbook, const Order& incoming) noexcept
{
	uint32_t matchCount = 0;
	Order order = incoming;

	LevelPriceType modifiedPrice = static_cast<int16_t>(order.price);
	auto& matchLevels = order.side == Side::BUY ? orderbook.sellLevels : orderbook.buyLevels;
	auto cond = get_cond(order.side);

	matchCount = process_orders(
		modifiedPrice,
		order.quantity,
		matchLevels,
		orderbook.orders,
		orderbook.ordersActive,
		cond
	);
	if (order.quantity > 0) [[unlikely]]
	{
		auto& sideLevels = order.side == Side::BUY ? orderbook.buyLevels : orderbook.sellLevels;
		orderbook.ordersActive.set(order.id);
		orderbook.orders[order.id] = order;
		add_order(modifiedPrice, order.id, order.side, order.quantity, sideLevels);
	}

	return matchCount;
}

// Templated helper to cancel an order within a given orders map.
inline __attribute((always_inline, hot)) void modify_order_in_book(
	PriceLevel& level,
	Order& order,
	QuantityType new_quantity
) noexcept
{
	level.volume += (new_quantity - order.quantity);
	if (new_quantity != 0) [[likely]]
	{
		order.quantity = new_quantity;
	}
	else [[unlikely]]
	{
		level.find_and_remove_order(order.id);
	}
}

void modify_order_by_id(Orderbook& orderbook, IdType order_id, QuantityType new_quantity) noexcept
{
	if (!orderbook.ordersActive[order_id]) [[unlikely]]
	{
		// ERROR_LOG("Order with ID " << order_id << " not found in modify_order_by_id.");
		return;
	}
	auto& order = orderbook.orders[order_id];

	auto& levels = order.side == Side::SELL ? orderbook.sellLevels : orderbook.buyLevels;
	auto it = std::find_if(
		levels.begin(),
		levels.end(),
		[order](const auto& level)
		{
			LevelPriceType compPrice = order.side == Side::BUY
				? -(static_cast<LevelPriceType>(order.price))
				: static_cast<LevelPriceType>(order.price);
			return level.first == compPrice;
		}
	);
	modify_order_in_book(it->second, order, new_quantity);

	if (it->second.volume == 0)
		levels.erase(it);

	if (new_quantity == 0) [[likely]]
	{
		orderbook.ordersActive.reset(order_id);
	}
	else [[unlikely]]
		order.quantity = new_quantity; // Update quantity in orders array
}

inline Order lookup_order_in_book(const Orders& orders, IdType order_id) noexcept
{
	return orders[order_id];
}

uint32_t get_volume_at_level(Orderbook& orderbook, Side side, PriceType price) noexcept
{
	auto& levels = side == Side::SELL ? orderbook.sellLevels : orderbook.buyLevels;
	auto it = std::find_if(
		levels.begin(),
		levels.end(),
		[price, side](const auto& level)
		{
			LevelPriceType compPrice = side == Side::BUY ? -(static_cast<LevelPriceType>(price))
														 : static_cast<LevelPriceType>(price);
			return level.first == compPrice;
		}
	);
	return it != levels.end() ? it->second.volume : 0;
}

// Functions below here don't need to be performant. Just make sure they're
// correct
Order lookup_order_by_id(Orderbook& orderbook, IdType order_id)
{
	if (!orderbook.ordersActive[order_id])
		throw std::runtime_error("Order not found");

	return lookup_order_in_book(orderbook.orders, order_id);

}

bool order_exists(Orderbook& orderbook, IdType order_id)
{
	return orderbook.ordersActive[order_id];
}

Orderbook* create_orderbook() { return new Orderbook; }
