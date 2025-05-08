#include "engine.hpp"
#include <algorithm>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

// Debug tracking
#define DEBUG_LOG(msg)                                                                             \
	std::cerr << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " - " << msg << std::endl
#define ERROR_LOG(msg)                                                                             \
	std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__ << " - " << msg << std::endl

inline void PriceLevel::add_order(Order& order)
{
	volume += order.quantity;
	orders.push_back(order.id);
}

inline void PriceLevel::fill_front_order(QuantityType quantity)
{
	volume -= quantity;
	orders.pop_front();
}

inline void PriceLevel::find_and_remove_order(Order& order)
{
	auto it = std::find(orders.begin(), orders.end(), order.id);
	if (it == orders.end())
		throw std::runtime_error("Order id does not exist in level");

	orders.erase(it);
}

// Overload operator<< for Side
inline std::ostream& operator<<(std::ostream& os, const Side& side)
{
	switch (side)
	{
	case Side::BUY:
		os << "BUY";
		break;
	case Side::SELL:
		os << "SELL";
		break;
	default:
		os << "UNKNOWN";
		break;
	}
	return os;
}

// Helper function to safely check array index access
template <typename T, size_t N> bool is_valid_index(const std::array<T, N>& array, size_t index)
{
	return index < N;
}

// Safe array access with bounds checking
template <typename T, size_t N>
T& safe_array_access(std::array<T, N>& array, size_t index, const char* context)
{
	if (!is_valid_index(array, index))
	{
		std::stringstream ss;
		ss << "Array index out of bounds: " << index << " >= " << N << " in " << context;
		ERROR_LOG(ss.str());
		throw std::out_of_range(ss.str());
	}
	return array[index];
}

inline PriceType get_price_index(PriceType price, PriceType currentBase)
{
	PriceType index = (price - currentBase) & BUFFER_MASK;
	if (index >= BUFFER_SIZE)
	{
		ERROR_LOG(
			"Price index out of range: " << index << " for price " << price << " and base "
										 << currentBase
		);
		return 0; // Safe fallback
	}
	return index;
}

inline PriceType get_base_price(PriceType centrePrice) { return centrePrice - (BUFFER_SIZE / 2); }

inline bool price_in_buffer_range(PriceType price, PriceType currentBase)
{
	int32_t relativePosition = (price >= currentBase)
		? static_cast<int32_t>(price) - static_cast<int32_t>(currentBase)
		: -1;
	return (relativePosition >= 0 && relativePosition < BUFFER_SIZE);
}

inline PriceType calculate_ideal_base_price(PriceType target_center_price) {
    if (target_center_price <= MIN_PRICE_FOR_NON_ZERO_BASE) {
        return 0;
    }
    return target_center_price - (BUFFER_SIZE / 2);
}

// Adjust the buffer range to ensure it always contains the best prices
template <typename PriceBuffer, typename OutlierMap>
void repopulate_buffer(PriceType& basePrice, PriceBuffer& levels, OutlierMap& outliers)
{
	try
	{
		size_t activeLevels = 0;
		for (auto& level : levels)
		{
			if (level.volume > 0)
			{
				activeLevels++;
			}
		}
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
					PriceType idx = get_price_index(it->first, newBasePrice);
					if (idx < BUFFER_SIZE)
					{
						levels[idx] = std::move(it->second);
						it = outliers.erase(it);
					}
					else
					{
						ERROR_LOG("Invalid buffer index during repopulate: " << idx);
						++it;
					}
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

			if (newBasePrice != basePrice)
			{
				// Create temporary storage for all active levels
				auto allLevels = outliers;
				outliers.clear();

				// Move buffer levels to temporary storage
				for (uint16_t i = 0; i < BUFFER_SIZE; i++)
				{
					if (levels[i].count() > 0)
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
						PriceType idx = get_price_index(price, basePrice);
						if (idx < BUFFER_SIZE)
						{
							levels[idx] = std::move(level);
						}
						else
						{
							ERROR_LOG("Invalid buffer index during redistribution: " << idx);
							outliers[price] = std::move(level); // Put in outliers instead
						}
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
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in repopulate_buffer: " << e.what());
	}
}

template <typename PriceBuffer, typename OutlierMap>
void adjust_buffer(
	PriceType newCenterPrice,
	PriceType& currentBase,
	PriceBuffer& levels,
	OutlierMap& outliers
)
{
	try
	{
		int32_t newBasePrice = get_base_price(newCenterPrice);
		int32_t shift = newBasePrice - currentBase;

		if (shift == 0)
			return;

		auto tempBuffer = levels;
		for (auto& level : levels)
		{
			level.volume = 0;
			level.orders.clear();
		}

		// Remap the old data to the new positions
		for (size_t i = 0; i < BUFFER_SIZE; i++)
		{
			int32_t oldPrice = currentBase + i;
			if (price_in_buffer_range(oldPrice, newBasePrice))
			{
				size_t newIndex = get_price_index(oldPrice, newBasePrice);
				if (newIndex < BUFFER_SIZE)
				{
					levels[newIndex] = std::move(tempBuffer[i]);
				}
				else
				{
					ERROR_LOG("Invalid index during buffer adjustment: " << newIndex);
					outliers[oldPrice] = std::move(tempBuffer[i]);
				}
			}
			else
			{
				outliers[oldPrice] = std::move(tempBuffer[i]);
			}
		}

		currentBase = newBasePrice;
	}
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in adjust_buffer: " << e.what());
	}
}

// Add an order to the orderbook
template <typename PriceBuffer, typename OutlierMap>
void add_order(
	Order& order,
	PriceType& basePrice,
	std::array<std::optional<Order>, MAX_ORDERS>& orders,
	PriceBuffer& levels,
	OutlierMap& outliers
)
{
	try
	{
		// Check order ID is valid
		if (order.id >= MAX_ORDERS)
		{
			ERROR_LOG("Invalid order ID: " << order.id << " >= " << MAX_ORDERS);
			return;
		}

		// Store the order in the orders array
		orders[order.id] = order;

		// Determine if the order should go in the buffer or outliers
		// If buffer is empty, this becomes the first order
		if (basePrice == 0) [[unlikely]]
		{
			basePrice = get_base_price(order.price);
			size_t index = get_price_index(order.price, basePrice);
			if (index >= BUFFER_SIZE)
			{
				ERROR_LOG("Invalid price index during first order: " << index);
				return;
			}
			auto& level = levels[index];
			if (level.count() >= MAX_ORDERS_PER_LEVEL)
			{
				ERROR_LOG("Price level overflow on first order");
				return;
			}
			level.add_order(order);
			return;
		}

		// DEBUG_LOG("active levels " + std::to_string(activeLevels));
		// If new ask or bid is worse than whats in buffer put in outliers
		if ((order.price < basePrice && order.side == Side::BUY) ||
			(order.price > basePrice + BUFFER_SIZE - 1 && order.side == Side::SELL)) [[unlikely]]
		{
			auto& level = outliers[order.price];
			// if (level.count() >= MAX_ORDERS_PER_LEVEL) {
			//     ERROR_LOG("Outlier overflow");
			// }
			level.add_order(order);
			return;
		}

		// if new price is better than what in buffer -> shift buffer
		if (!price_in_buffer_range(order.price, basePrice)) [[unlikely]]
		{
			adjust_buffer(order.price, basePrice, levels, outliers);
		}

		size_t index = get_price_index(order.price, basePrice);

		auto& level = levels[index];
		level.add_order(order);
	}
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in add_order: " << e.what());
	}
}

// Match orders at a specific price level
inline uint32_t match_price_level(
	Order& incoming,
	PriceLevel& level,
	std::array<std::optional<Order>, MAX_ORDERS>& orders
)
{
	try
	{
		uint32_t matchCount = 0;

		// Quick check if level is empty
		if (level.count() == 0 || level.volume == 0)
		{
			return 0;
		}

		while (!level.orders.empty() && incoming.quantity > 0)
		{
			IdType orderId = level.orders.front();

			auto& counterOrder = orders[orderId];
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
				level.orders.pop_front();
			}
		}

		return matchCount;
	}
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in match_price_level: " << e.what());
		return 0;
	}
}

// Process matching orders
template <typename PriceBuffer, typename OutlierMap>
uint32_t process_orders(
	Order& incoming,
	PriceType& basePrice,
	std::array<std::optional<Order>, MAX_ORDERS>& orders,
	PriceBuffer& levels,
	OutlierMap& outliers
)
{
	try
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
				if (level.count() == 0)
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
		if (matchCount > 0) [[unlikely]]
		{
		    repopulate_buffer(basePrice, levels, outliers);
		}

		return matchCount;
	}
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in process_orders: " << e.what());
		return 0;
	}
}

uint32_t match_order(Orderbook& orderbook, const Order& incoming)
{
	try
	{
		int activeBuyLevels = orderbook.activeLevels(Side::BUY);
		int activeSellLevels = orderbook.activeLevels(Side::SELL);

		// std::stringstream ss;
		// ss << "Incoming side: " << incoming.side;
		// DEBUG_LOG(ss.str());
		// DEBUG_LOG("Incoming price: " + std::to_string(incoming.price));
		// DEBUG_LOG("Base buy price: " + std::to_string(orderbook.baseBuyPrice));
		// DEBUG_LOG("Base sell price: " + std::to_string(orderbook.baseSellPrice));
		// DEBUG_LOG("Active buy levels:" + std::to_string(activeBuyLevels));
		// DEBUG_LOG("Outlier buy levels:" + std::to_string(orderbook.buyOutliers.size()));
		// DEBUG_LOG("Active sell levels:" + std::to_string(activeSellLevels));
		// DEBUG_LOG("Outlier sell levels:" + std::to_string(orderbook.sellOutliers.size()));

		// Check if the order is valid
		if (incoming.id >= MAX_ORDERS)
		{
			ERROR_LOG("Invalid incoming order ID: " << incoming.id << " >= " << MAX_ORDERS);
			return 0;
		}

		uint32_t matchCount = 0;
		Order order = incoming; // Create a copy to modify the quantity

		if (order.side == Side::BUY)
		{
			// For a BUY, match with sell orders priced at or below the order's price.
			matchCount = process_orders(
				order,
				orderbook.baseSellPrice,
				orderbook.orders,
				orderbook.sellLevels,
				orderbook.sellOutliers
			);
			if (order.quantity > 0)
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
			matchCount = process_orders(
				order,
				orderbook.baseBuyPrice,
				orderbook.orders,
				orderbook.buyLevels,
				orderbook.buyOutliers
			);
			// For a SELL, match with buy orders priced at or above the order's price.
			if (order.quantity > 0)
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
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in match_order: " << e.what());
		return 0;
	}
}

// Templated helper to cancel an order within a given orders map.
template <typename PriceBuffer, typename OutlierMap>
bool modify_order(
	Order& order,
	QuantityType new_quantity,
	PriceType& basePrice,
	PriceBuffer& levels,
	OutlierMap& outliers
)
{
	try
	{
		// Update volume at price level
		PriceLevel* level = nullptr;
		if (price_in_buffer_range(order.price, basePrice))
		{
			size_t index = get_price_index(order.price, basePrice);
			// if (index >= BUFFER_SIZE) {
			//     ERROR_LOG("Invalid price index during modify: " << index);
			//     return false;
			// }
			level = &levels[index];
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
			level->find_and_remove_order(order);

			if (level->count() == 0) [[unlikely]]
			{
			    repopulate_buffer(basePrice,  levels, outliers);
			}
			return true;
		}
	}
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in modify_order: " << e.what());
		return false;
	}
}

void modify_order_by_id(Orderbook& orderbook, IdType order_id, QuantityType new_quantity)
{
	try
	{
		// Check if order ID is valid
		if (order_id >= MAX_ORDERS)
		{
			ERROR_LOG("Invalid order ID in modify_order_by_id: " << order_id);
			return;
		}

		auto& maybeOrder = orderbook.orders[order_id];
		if (!maybeOrder)
			return;

		if (maybeOrder->side == Side::BUY)
		{
			modify_order(
				*maybeOrder,
				new_quantity,
				orderbook.baseBuyPrice,
				orderbook.buyLevels,
				orderbook.buyOutliers
			);
		}
		else
		{
			modify_order(
				*maybeOrder,
				new_quantity,
				orderbook.baseSellPrice,
				orderbook.sellLevels,
				orderbook.sellOutliers
			);
		}

		if (new_quantity == 0)
			maybeOrder = std::nullopt;
	}
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in modify_order_by_id: " << e.what());
	}
}

template <typename Orders> std::optional<Order> lookup_order(Orders& orders, IdType order_id)
{
	try
	{
		if (order_id >= MAX_ORDERS)
		{
			ERROR_LOG("Invalid order ID in lookup_order: " << order_id);
			return std::nullopt;
		}
		return orders[order_id];
	}
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in lookup_order: " << e.what());
		return std::nullopt;
	}
}

uint32_t get_volume_at_level(Orderbook& orderbook, Side side, PriceType price)
{
	try
	{
		if (side == Side::BUY)
		{
			if (price_in_buffer_range(price, orderbook.baseBuyPrice)) [[likely]]
			{
				size_t index = get_price_index(price, orderbook.baseBuyPrice);
				if (index >= BUFFER_SIZE)
				{
					ERROR_LOG("Invalid price index in get_volume_at_level: " << index);
					return 0;
				}
				return orderbook.buyLevels[index].volume;
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
				size_t index = get_price_index(price, orderbook.baseSellPrice);
				if (index >= BUFFER_SIZE)
				{
					ERROR_LOG("Invalid price index in get_volume_at_level: " << index);
					return 0;
				}
				return orderbook.sellLevels[index].volume;
			}
			else if (orderbook.sellOutliers.count(price)) [[unlikely]]
			{
				return orderbook.sellOutliers[price].volume;
			}
		}
		return 0;
	}
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in get_volume_at_level: " << e.what());
		return 0;
	}
}

// Functions below here don't need to be performant. Just make sure they're
// correct
Order lookup_order_by_id(Orderbook& orderbook, IdType order_id)
{
	try
	{
		if (order_id >= MAX_ORDERS)
		{
			ERROR_LOG("Invalid order ID in lookup_order_by_id: " << order_id);
			throw std::runtime_error("Order not found");
		}

		auto order = lookup_order(orderbook.orders, order_id);
		if (order.has_value())
			return *order;
		throw std::runtime_error("Order not found");
	}
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in lookup_order_by_id: " << e.what());
		throw;
	}
}

bool order_exists(Orderbook& orderbook, IdType order_id)
{
	try
	{
		if (order_id >= MAX_ORDERS)
		{
			ERROR_LOG("Invalid order ID in order_exists: " << order_id);
			return false;
		}

		auto order = lookup_order(orderbook.orders, order_id);
		return order.has_value();
	}
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in order_exists: " << e.what());
		return false;
	}
}

Orderbook* create_orderbook()
{
	try
	{
		return new Orderbook();
	}
	catch (const std::exception& e)
	{
		ERROR_LOG("Exception in create_orderbook: " << e.what());
		return nullptr;
	}
}