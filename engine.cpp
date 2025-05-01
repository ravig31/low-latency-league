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

PriceType get_price_index(PriceType price, PriceType currentBase)
{
	return (price - currentBase) & BUFFER_MASK;
}

PriceType get_base_price(PriceType price){
	return price - (BUFFER_SIZE / 2);
}


bool price_in_buffer_range(PriceType price, PriceType currentBase)
{
	PriceType relativePosition = price - currentBase;
	return (relativePosition >= 0 && relativePosition < BUFFER_SIZE);
}

template <typename PriceBuffer, typename OutlierMap>
PriceLevel& get_price_level(
	PriceType price,
	PriceType& currentBase,
	PriceBuffer& buffer,
	OutlierMap& outliers
)
{
	if (!price_in_buffer_range(price, currentBase))
	{
		return buffer[get_price_index(price, currentBase)];
	}
	else
	{
		// Return from overflow map, creating entry if needed
		return outliers[price];
	}
}

template <typename PriceBuffer, typename OutlierMap>
void adjust_buffer(
	PriceType newCenterPrice,
	PriceType& currentBase,
	PriceBuffer& buffer,
	OutlierMap& outliers
)
{
	int32_t newBasePrice = get_base_price(newCenterPrice);
	int32_t shift = newBasePrice - currentBase;

	if (shift == 0)
		return;

	auto tempBuffer = buffer;
	for (auto& level : buffer)
	{
		level.volume = 0;
		level.count = 0;
	}

	// Remap the old data to the new positions
	for (size_t i = 0; i < BUFFER_SIZE; i++)
	{
		int32_t oldPrice = currentBase + i;
		if (price_in_buffer_range(oldPrice, currentBase))
		{
			int32_t newIndex = get_price_index(oldPrice, newBasePrice);
			if (newIndex >= 0 && newIndex < BUFFER_SIZE)
			{
				buffer[newIndex] = tempBuffer[i];
			}
			else
			{
				outliers[oldPrice] = tempBuffer[i];
			}
		}
	}

	currentBase = newBasePrice;
}

// Add an order to a price level
template <typename PriceBuffer, typename OutlierMap, typename Orders>
void add_order(
	Order& order,
	PriceType& currentBase,
	Orders& orders,
	PriceBuffer& buffer,
	OutlierMap& outliers
)
{
	if (currentBase == 0){
		currentBase = get_base_price(order.price); 
	}

	if (!price_in_buffer_range(order.price, currentBase))
	{
		adjust_buffer(order.price, currentBase, buffer, outliers);
	}

	int32_t index = get_price_index(order.price, currentBase);

	// Ensure index is valid after adjustment
	if (index < 0 || index >= BUFFER_SIZE)
	{
		return;
	}

	auto& level = buffer[index];

	if (level.count >= MAX_ORDERS_PER_LEVEL)
	{
		return; // Level is full
	}

	orders[order.id] = order;
	level.orders[level.count++] = order.id;
	level.volume += order.quantity;

	return;
}

// template <typename Levels, typename Orders, typename Condition>
// inline __attribute__((always_inline, hot)) uint32_t
// process_orders(Order& order, Orders& orders, Levels& levels, Condition cond)
// {
// 	uint32_t matchCount = 0;

// 	// for (PriceType i = 0; i < MAX_PRICE && order.quantity > 0; ++i)
// 	// {

// 	// 	PriceType price = (order.side == Side::BUY) ? i : (MAX_PRICE - 1 - i);
// 	// 	auto& level = levels[price];

// 	// 	if (__builtin_expect(level.volume == 0, 0))
// 	// 		continue;

// 	// 	// Check if this price matches our condition
// 	// 	if (!(price == order.price || cond(order.price, price)))
// 	// 		break;

// 	// 	for (uint16_t i = 0; i < level.count && order.quantity > 0; ++i)
// 	// 	{
// 	// 		auto& matchingOrder = orders[level.orders[i]];

// 	// 		if (!matchingOrder)
// 	// 			continue;

// 	// 		QuantityType trade = std::min(order.quantity, matchingOrder->quantity);
// 	// 		order.quantity -= trade;
// 	// 		matchingOrder->quantity -= trade;
// 	// 		level.volume -= trade;
// 	// 		++matchCount;

// 	// 		if (matchingOrder->quantity == 0)
// 	// 		{
// 	// 			matchingOrder = std::nullopt;
// 	// 		}
// 	// 	}

// 	// 	// Compact the level by removing filled orders if needed
// 	// 	if (matchCount > 0)
// 	// 	{
// 	// 		compact_price_level(level);
// 	// 	}
// 	// }

// 	return matchCount;
// }

uint32_t match_order(Orderbook& orderbook, const Order& incoming)
{
	uint32_t matchCount = 0;
	Order order = incoming; // Create a copy to modify the quantity

	if (order.side == Side::BUY)
	{
		// For a BUY, match with sell orders priced at or below the order's price.
		add_order(
			order,
			orderbook.baseBuyPrice,
			orderbook.orders,
			orderbook.buyLevels,
			orderbook.buyOutliers
		);
	}
	else
	{ // Side::SELL
		// For a SELL, match with buy orders priced at or above the order's price.
		add_order(
			order,
			orderbook.baseSellPrice,
			orderbook.orders,
			orderbook.sellLevels,
			orderbook.sellOutliers
		);
	}
	return matchCount;
}

// // Templated helper to cancel an order within a given orders map.
// template <typename Levels>
// bool modify_order(Order& order, Levels& levels, IdType order_id, QuantityType new_quantity)
// {
// 	PriceType price = order.price;

// 	// Update volume at price level
// 	auto& level = levels[price];
// 	level.volume += (new_quantity - order.quantity);

// 	if (new_quantity != 0)
// 	{
// 		// Update order quantity
// 		order.quantity = new_quantity;
// 		return true;
// 	}
// 	else
// 	{
// 		auto& orders_at_level = level.orders;

// 		for (size_t i = 0; i < MAX_ORDERS_PER_LEVEL; i++)
// 		{
// 			if (orders_at_level[i] == order_id)
// 			{
// 				orders_at_level[i] = 0;
// 				break;
// 			}
// 		}
// 		return true;
// 	}
// }

// void modify_order_by_id(Orderbook& orderbook, IdType order_id, QuantityType new_quantity)
// {
// 	auto& maybeOrder = orderbook.orders[order_id];
// 	if (!maybeOrder)
// 		return;

// 	modify_order(
// 		*maybeOrder,
// 		maybeOrder->side == Side::BUY ? orderbook.buyLevels : orderbook.sellLevels,
// 		order_id,
// 		new_quantity
// 	);
// 	if (new_quantity == 0)
// 		maybeOrder = std::nullopt;
// }

// template <typename Orders> std::optional<Order> lookup_order(Orders& orders, IdType order_id)
// {
// 	return orders[order_id];
// }

// uint32_t get_volume_at_level(Orderbook& orderbook, Side side, PriceType price)
// {
// 	if (side == Side::BUY)
// 	{
// 		return orderbook.buyLevels[price].volume;
// 	}
// 	else if (side == Side::SELL)
// 	{
// 		return orderbook.sellLevels[price].volume;
// 	}
// 	return 0;
// }

// // Functions below here don't need to be performant. Just make sure they're
// // correct
// Order lookup_order_by_id(Orderbook& orderbook, IdType order_id)
// {
// 	auto order = lookup_order(orderbook.orders, order_id);
// 	if (order.has_value())
// 		return *order;
// 	throw std::runtime_error("Order not found");
// }

// bool order_exists(Orderbook& orderbook, IdType order_id)
// {
// 	auto order = lookup_order(orderbook.orders, order_id);
// 	return order.has_value();
// }

// Orderbook* create_orderbook() { return new Orderbook(); }
