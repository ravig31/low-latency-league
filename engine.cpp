#include "engine.hpp"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.

inline void add_order_to_level(PriceLevel& level, IdType order_id, QuantityType quantity)
{

	// Find first free slot and use it
	for (size_t i = 0; i < MAX_ORDERS_PER_LEVEL; i++)
	{
		if (level.orders[i] == 0) // Assuming 0 is invalid order ID
		{
			level.orders[i] = order_id;
			level.volume += quantity;
			level.count++;
			break;
		}
	}
}

inline void remove_order_from_level(PriceLevel& level, IdType order_id)
{
	for (size_t i = 0; i < MAX_ORDERS_PER_LEVEL; i++)
	{
		if (level.orders[i] == order_id)
		{
			level.orders[i] = 0;
			level.count--;
			break;
		}
	}
}

inline void compact_price_level(PriceLevel& level)
{
	uint16_t writeIdx = 0;
	for (uint16_t readIdx = 0; readIdx < MAX_ORDERS_PER_LEVEL; ++readIdx)
	{
		bool shouldKeep = (level.orders[readIdx] != 0);
		level.orders[writeIdx] =
			level.orders[readIdx] * shouldKeep + level.orders[writeIdx] * !shouldKeep;
		writeIdx += shouldKeep;
	}
	level.count = writeIdx;
}

template <typename Levels, typename Orders, typename Condition>
inline __attribute__((always_inline, hot)) uint32_t
process_orders(Order& order, Orders& orders, Levels& levels, Condition cond)
{
	uint32_t matchCount = 0;

	for (PriceType i = 0; i < MAX_PRICE && order.quantity > 0; ++i)
	{

		PriceType price = (order.side == Side::BUY) ? i : (MAX_PRICE - 1 - i);
		auto& level = levels[price];

		if (__builtin_expect(level.volume == 0, 0))
			continue;

		// Check if this price matches our condition
		if (!(price == order.price || cond(order.price, price)))
			break;

#pragma GCC unroll 4
		for (uint16_t i = 0; i < level.count && order.quantity > 0; ++i)
		{
			auto& matchingOrder = orders[level.orders[i]];

			if (!matchingOrder)
				continue;

			QuantityType trade = std::min(order.quantity, matchingOrder->quantity);
			order.quantity -= trade;
			matchingOrder->quantity -= trade;
			level.volume -= trade;
			++matchCount;

			if (matchingOrder->quantity == 0)
			{
				matchingOrder = std::nullopt;
			}
		}

		// Compact the level by removing filled orders if needed
		if (matchCount > 0)
		{
			compact_price_level(level);
		}
	}

	return matchCount;
}

uint32_t match_order(Orderbook& orderbook, const Order& incoming)
{
	uint32_t matchCount = 0;
	Order order = incoming; // Create a copy to modify the quantity

	if (order.side == Side::BUY)
	{
		// For a BUY, match with sell orders priced at or below the order's price.
		matchCount = process_orders(
			order,
			orderbook.orders,
			orderbook.sellLevels,
			std::greater<PriceType>()
		);
		if (order.quantity > 0)
		{
			orderbook.orders[order.id] = order;
			auto& level = orderbook.buyLevels[order.price];
			add_order_to_level(level, order.id, order.quantity);
		}
	}
	else
	{ // Side::SELL
		// For a SELL, match with buy orders priced at or above the order's price.
		matchCount =
			process_orders(order, orderbook.orders, orderbook.buyLevels, std::less<PriceType>());
		if (order.quantity > 0)
		{
			orderbook.orders[order.id] = order;
			auto& level = orderbook.sellLevels[order.price];
			add_order_to_level(level, order.id, order.quantity);
		}
	}
	return matchCount;
}

// Templated helper to cancel an order within a given orders map.
template <typename Levels>
bool modify_order(Order& order, Levels& levels, IdType order_id, QuantityType new_quantity)
{
	PriceType price = order.price;

	// Update volume at price level
	auto& level = levels[price];
	level.volume += (new_quantity - order.quantity);

	if (new_quantity != 0)
	{
		// Update order quantity
		order.quantity = new_quantity;
		return true;
	}
	else
	{
		auto& orders_at_level = level.orders;

		for (size_t i = 0; i < MAX_ORDERS_PER_LEVEL; i++)
		{
			if (orders_at_level[i] == order_id)
			{
				orders_at_level[i] = 0;
				break;
			}
		}
		return true;
	}
}

void modify_order_by_id(Orderbook& orderbook, IdType order_id, QuantityType new_quantity)
{
	auto& maybeOrder = orderbook.orders[order_id];
	if (!maybeOrder)
		return;

	modify_order(
		*maybeOrder,
		maybeOrder->side == Side::BUY ? orderbook.buyLevels : orderbook.sellLevels,
		order_id,
		new_quantity
	);
	if (new_quantity == 0)
		maybeOrder = std::nullopt;
}

template <typename Orders> std::optional<Order> lookup_order(Orders& orders, IdType order_id)
{
	return orders[order_id];
}

uint32_t get_volume_at_level(Orderbook& orderbook, Side side, PriceType price)
{
	if (side == Side::BUY)
	{
		return orderbook.buyLevels[price].volume;
	}
	else if (side == Side::SELL)
	{
		return orderbook.sellLevels[price].volume;
	}
	return 0;
}

// Functions below here don't need to be performant. Just make sure they're
// correct
Order lookup_order_by_id(Orderbook& orderbook, IdType order_id)
{
	auto order = lookup_order(orderbook.orders, order_id);
	if (order.has_value())
		return *order;
	throw std::runtime_error("Order not found");
}

bool order_exists(Orderbook& orderbook, IdType order_id)
{
	auto order = lookup_order(orderbook.orders, order_id);
	return order.has_value();
}

Orderbook* create_orderbook()
{
	Orderbook* ob = static_cast<Orderbook*>(malloc(sizeof(Orderbook)));
	if (ob == nullptr)
	{
		// Handle allocation failure!
		perror("Failed to allocate memory for Orderbook");
		return nullptr;
	}
	// The constructor of Orderbook will be called here, initializing buyLevels, sellLevels, and
	// orders.
	return ob;
}
