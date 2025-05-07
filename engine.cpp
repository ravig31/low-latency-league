#include "engine.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.

inline PriceType get_price_index(PriceType price, PriceType currentBase)
{
	return (price - currentBase) & BUFFER_MASK;
}

inline PriceType get_base_price(PriceType price) { return price - (BUFFER_SIZE / 2); }

inline bool price_in_buffer_range(PriceType price, PriceType currentBase)
{
	int32_t relativePosition = (price >= currentBase) ? static_cast<int32_t>(price) - static_cast<int32_t>(currentBase) : -1;
	return (relativePosition >= 0 && relativePosition < BUFFER_SIZE);
}

// Adjust the buffer range to ensure it always contains the best prices
template <typename PriceBuffer, typename OutlierMap>
void repopulate_buffer(
	PriceType& basePrice,
	size_t& activeLevels,
	PriceBuffer& levels,
	OutlierMap& outliers
)
{

	// Check if we need to recenter the buffer
	if (activeLevels == 0 && !outliers.empty())
	{
		// Buffer is empty but outliers exist - reset base price to best outlier
		PriceType newBasePrice = get_base_price(outliers.begin()->first);

		// Move appropriate outliers into the buffer
		auto it = outliers.begin();
		while (it != outliers.end())
		{

			if (price_in_buffer_range(it->first, newBasePrice))
			{
				// Move from outliers to buffer
				levels[get_price_index(it->first, newBasePrice)] = std::move(it->second);
				activeLevels++;
				it = outliers.erase(it);
			}
			else
			{
				++it;
			}
		}

		basePrice = newBasePrice;
	}
	else if (activeLevels < BUFFER_SIZE / 4 && !outliers.empty())
	{
		// Buffer is sparsely populated - consider recentering
		PriceType newBasePrice = get_base_price(outliers.begin()->first);
		activeLevels = 0;

		if (newBasePrice != basePrice)
		{
			// Create temporary storage for all active levels
			auto allLevels = outliers;
			outliers.clear();

			// Move buffer levels to temporary storage
			for (uint16_t i = 0; i < BUFFER_SIZE; i++)
			{
				if (levels[i].count > 0)
				{
					PriceType levelPrice = basePrice + i;
					allLevels[levelPrice] = std::move(levels[i]);
					levels[i] = PriceLevel{}; // Reset the level
				}
			}

			// Reset base price
			basePrice = newBasePrice;

			// Redistribute levels between buffer and outliers
			for (auto& [price, level] : allLevels)
			{

				if (price_in_buffer_range(price, basePrice))
				{
					// In buffer range
					levels[get_price_index(price, basePrice)] = std::move(level);
					activeLevels++;
				}
				else
				{
					// In outlier range
					outliers[price] = std::move(level);
				}
			}
		}
	}
}

template <typename PriceBuffer, typename OutlierMap>
void adjust_buffer(
	PriceType newCenterPrice,
	PriceType& currentBase,
	size_t& activeLevels,
	PriceBuffer& levels,
	OutlierMap& outliers
)
{
	int32_t newBasePrice = get_base_price(newCenterPrice);
	int32_t shift = newBasePrice - currentBase;

	if (shift == 0)
		return;

	auto tempBuffer = levels;
	for (auto& level : levels)
	{
		level.volume = 0;
		level.count = 0;
	}

	// Remap the old data to the new positions
	activeLevels = 0;
	for (size_t i = 0; i < BUFFER_SIZE; i++)
	{
		int32_t oldPrice = currentBase + i;
		if (price_in_buffer_range(oldPrice, newBasePrice))
		{
			size_t newIndex = get_price_index(oldPrice, newBasePrice);
			levels[newIndex] = std::move(tempBuffer[i]);
			activeLevels++;
		}
		else
		{
			outliers[oldPrice] = std::move(tempBuffer[i]);
		}
	}

	currentBase = newBasePrice;
}

// Add an order to the orderbook
template <typename PriceBuffer, typename OutlierMap>
void add_order(
	Order& order,
	PriceType& basePrice,
	size_t& activeLevels,
	std::array<std::optional<Order>, MAX_ORDERS>& orders,
	PriceBuffer& levels,
	OutlierMap& outliers
)
{
	// Store the order in the orders array
	orders[order.id] = order;

	// Determine if the order should go in the buffer or outliers
	// If buffer is empty, this becomes the first order
	if (basePrice == 0) [[unlikely]]
	{
		basePrice = get_base_price(order.price);
		auto& level = levels[get_price_index(order.price, basePrice)];
		level.orders[level.count++] = order.id;
		level.volume += order.quantity;
		activeLevels++;
		return;
	}

	// If new ask or bid is worse than whats in buffer put in outliers
	if ((order.price < basePrice && order.side == Side::BUY) ||
		(order.price > basePrice + BUFFER_SIZE - 1 && order.side == Side::SELL)) [[unlikely]]
	{
		auto& level = outliers[order.price];
		level.orders[level.count++] = order.id;
		level.volume += order.quantity;
		return;
	}

	// if new price is better than what in buffer -> shift buffer
	if (!price_in_buffer_range(order.price, basePrice)) [[unlikely]]
	{
		adjust_buffer(order.price, basePrice, activeLevels, levels, outliers);
	}

	size_t index = get_price_index(order.price, basePrice);

	auto& level = levels[index];
	if (level.count < MAX_ORDERS_PER_LEVEL) [[likely]]
	{
		level.orders[level.count++] = order.id;
		level.volume += order.quantity;
		activeLevels++;
	}
	else [[unlikely]]
	{
		// Handle the unlikely case of level overflow
		// This should rarely happen with MAX_ORDERS_PER_LEVEL = 512
		throw std::runtime_error("Price level overflow");
	}
}

// Match orders at a specific price level
inline uint32_t match_price_level(
	Order& incoming,
	PriceLevel& level,
	std::array<std::optional<Order>, MAX_ORDERS>& orders
)
{
	uint32_t matchCount = 0;

	// Quick check if level is empty
	if (level.count == 0 || level.volume == 0)
	{
		return 0;
	}

	// Match orders at this price level
	for (uint16_t i = 0; i < level.count && incoming.quantity > 0;)
	{
		IdType orderId = level.orders[i];
		auto& counterOrder = orders[orderId];

		if (!counterOrder)
		{
			// Invalid order ID, remove from level
			level.orders[i] = level.orders[--level.count];
			continue;
		}

		// Calculate matched quantity
		QuantityType matchQty = std::min(incoming.quantity, counterOrder->quantity);

		// Update quantities
		incoming.quantity -= matchQty;
		counterOrder->quantity -= matchQty;
		level.volume -= matchQty;

		// Count this match
		matchCount++;

		if (counterOrder->quantity == 0)
		{
			// Counter order fully matched, remove it
			orders[orderId] = std::nullopt;
			level.orders[i] = level.orders[--level.count];
		}
		else
		{
			// Counter order partially matched, move to next
			i++;
		}
	}

	return matchCount;
}

// Process matching orders
template <typename PriceBuffer, typename OutlierMap>
uint32_t process_orders(
	Order& incoming,
	PriceType& basePrice,
	size_t& activeLevels,
	std::array<std::optional<Order>, MAX_ORDERS>& orders,
	PriceBuffer& levels,
	OutlierMap& outliers
)
{
	uint32_t matchCount = 0;

	// Nothing to match if the counterparty side is empty
	if (basePrice == 0)
	{
		return 0;
	}

	// For buys: match if sell price <= buy price
	// For sells: match if buy price >= sell price
	auto should_match = [&](PriceType counterPrice)
	{
		return incoming.side == Side::BUY ? incoming.price >= counterPrice
										  : incoming.price <= counterPrice;
	};

	for (size_t idx = 0; idx < BUFFER_SIZE && incoming.quantity > 0; idx++) [[likely]]
	{
		size_t priceIdx = incoming.side == Side::BUY ? idx : (BUFFER_SIZE - 1) - idx;
		if (levels[priceIdx].volume == 0)
			continue;

		PriceType currentPrice = basePrice + priceIdx;
		if (!should_match(currentPrice))
		{
			break; // Price no longer matches
		}

		auto& level = levels[priceIdx];
		matchCount += match_price_level(incoming, level, orders);

		if (level.volume == 0)
			activeLevels--;
	}

	// If incoming order still has quantity, try to match with outliers
	if (incoming.quantity > 0) [[unlikely]]
	{
		for (auto it = outliers.begin(); it != outliers.end() && incoming.quantity > 0;)
		{
			PriceType currentPrice = it->first;

			if (!should_match(currentPrice))
			{
				break; // Price no longer matches
			}

			auto& level = it->second;
			uint32_t levelMatches = match_price_level(incoming, level, orders);
			matchCount += levelMatches;

			// If level is now empty, remove it
			if (level.count == 0)
			{
				it = outliers.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	// After matching, shift the buffer range if needed
	if (activeLevels < BUFFER_SIZE / 4 && !outliers.empty() && matchCount > 0) [[unlikely]]
	{
		repopulate_buffer(basePrice, activeLevels, levels, outliers);
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
			orderbook.baseSellPrice,
			orderbook.activeSellLevels,
			orderbook.orders,
			orderbook.sellLevels,
			orderbook.sellOutliers
		);
		if (order.quantity > 0)
			add_order(
				order,
				orderbook.baseBuyPrice,
				orderbook.activeBuyLevels,
				orderbook.orders,
				orderbook.buyLevels,
				orderbook.buyOutliers
			);
	}
	else
	{ // Side::SELL
		matchCount = process_orders(
			order,
			orderbook.baseBuyPrice,
			orderbook.activeBuyLevels,
			orderbook.orders,
			orderbook.buyLevels,
			orderbook.buyOutliers
		);
		// For a SELL, match with buy orders priced at or above the order's price.
		if (order.quantity > 0)
			add_order(
				order,
				orderbook.baseSellPrice,
				orderbook.activeSellLevels,
				orderbook.orders,
				orderbook.sellLevels,
				orderbook.sellOutliers
			);
	}
	return matchCount;
}

// Templated helper to cancel an order within a given orders map.
template <typename PriceBuffer, typename OutlierMap>
bool modify_order(
	Order& order,
	IdType order_id,
	QuantityType new_quantity,
	size_t& activeLevels,
	PriceType& basePrice,
	PriceBuffer& levels,
	OutlierMap& outliers
)
{

	// Update volume at price level
	PriceLevel* level;
	if (price_in_buffer_range(order.price, basePrice))
	{
		level = &levels[get_price_index(order.price, basePrice)];
		if (level->volume == 0)
			return false;
	}
	else if (outliers.count(order.price))
	{
		level = &outliers[order.price];
	}
	else
	{
		return false;
	}

	level->volume += (new_quantity - order.quantity);

	if (new_quantity != 0)
	{
		// Update order quantity
		order.quantity = new_quantity;
		return true;
	}
	else
	{
		for (size_t i = 0; i < level->count; i++)
		{
			if (level->orders[i] == order_id)
			{
				std::swap(level->orders[i], level->orders[level->count]);
				level->orders[level->count] = 0;
				level->count--;
				break;
			}
		}
		if (level->count == 0) activeLevels--;

		if (activeLevels < BUFFER_SIZE / 4 && !outliers.empty() && level->count == 0) [[unlikely]]
		{
			repopulate_buffer(basePrice, activeLevels, levels, outliers);
		}
		return true;
	}
}

void modify_order_by_id(Orderbook& orderbook, IdType order_id, QuantityType new_quantity)
{
	auto& maybeOrder = orderbook.orders[order_id];
	if (!maybeOrder)
		return;

	if (maybeOrder->side == Side::BUY)
	{
		modify_order(
			*maybeOrder,
			order_id,
			new_quantity,
			orderbook.activeSellLevels,
			orderbook.baseBuyPrice,
			orderbook.buyLevels,
			orderbook.buyOutliers
		);
	}
	else
	{
		modify_order(
			*maybeOrder,
			order_id,
			new_quantity,
			orderbook.activeSellLevels,
			orderbook.baseSellPrice,
			orderbook.sellLevels,
			orderbook.sellOutliers
		);
	}

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
		if (price_in_buffer_range(price, orderbook.baseBuyPrice)) [[likely]]
		{
			return orderbook.buyLevels[get_price_index(price, orderbook.baseBuyPrice)].volume;
		}
		else if (orderbook.buyOutliers.count(price)) [[unlikely]]
		{
			return orderbook.buyOutliers[price].volume;
		}
	}
	else if (side == Side::SELL)
	{
		if (price_in_buffer_range(price, orderbook.baseSellPrice)) [[likely]]
		{
			return orderbook.sellLevels[get_price_index(price, orderbook.baseSellPrice)].volume;
		}
		else if (orderbook.sellOutliers.count(price)) [[unlikely]]
		{
			return orderbook.sellOutliers[price].volume;
		}
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

Orderbook* create_orderbook() { return new Orderbook(); }
