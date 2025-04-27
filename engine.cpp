#include "engine.hpp"
#include <algorithm>
#include <cstddef>
#include <deque>
#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <vector>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.

template <typename Condition>
void add_order(
	Order& order,
	std::vector<PriceType>& prices,
	std::vector<VolumeType>& volumes,
	std::vector<std::deque<Order>>& orders,
	Condition cond
)
{
	if (prices.empty())
	{
		prices.push_back(order.price);
		volumes.push_back(order.quantity);
		orders.push_back({ order });
		return;
	}

	auto rit = prices.rbegin();
	while (rit != prices.rend() && cond(order.price, *rit))
	{
		++rit;
	}


	size_t index;
	if (rit != prices.rend() && *rit == order.price)
	{
		index = std::distance(prices.begin(), rit.base()) - 1;
		orders[index].push_back(order);
		volumes[index] += order.quantity;
	}
	else
	{
        index = std::distance(prices.begin(), rit.base());
        
        prices.insert(rit.base(), order.price);
        volumes.insert(volumes.begin() + index, order.quantity);
        orders.insert(orders.begin() + index, std::deque<Order>{ order });
	}
}

template <typename Condition>
uint32_t process_orders(
	Order& order,
	std::vector<PriceType>& prices,
	std::vector<VolumeType>& volumes,
	std::vector<std::deque<Order>>& orders,
	Condition cond
)
{
	uint32_t matchCount = 0;
	auto rit = prices.rbegin();
	while (rit != prices.rend() && order.quantity > 0 &&
		   (*rit == order.price || cond(order.price, *rit)))
	{
		size_t index = std::distance(prices.begin(), rit.base()) - 1;
		auto& ordersAtLevel = orders[index];
		while (!ordersAtLevel.empty() && order.quantity > 0)
		{
			QuantityType trade = std::min(order.quantity, ordersAtLevel.front().quantity);
			order.quantity -= trade;
			ordersAtLevel.front().quantity -= trade;
			volumes[index] -= trade; // Decrement total volume at level
			if (ordersAtLevel.front().quantity == 0)
				ordersAtLevel.pop_front();
			++matchCount;
		}
		if (ordersAtLevel.empty())
		{
			volumes.erase(volumes.begin() + index);
			orders.erase(orders.begin() + index);
			rit = std::make_reverse_iterator(prices.erase(--rit.base()));
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
			orderbook.sellPrices,
			orderbook.sellVolumes,
			orderbook.sellOrders,
			std::greater<PriceType>()
		);
		if (order.quantity > 0)
			add_order(
				order,
				orderbook.buyPrices,
				orderbook.buyVolumes,
				orderbook.buyOrders,
				std::less<PriceType>()
			);
	}
	else
	{ // Side::SELL
		// For a SELL, match with buy orders priced at or above the order's price.
		matchCount = process_orders(
			order,
			orderbook.buyPrices,
			orderbook.buyVolumes,
			orderbook.buyOrders,
			std::less<PriceType>()
		);
		if (order.quantity > 0)
			add_order(
				order,
				orderbook.sellPrices,
				orderbook.sellVolumes,
				orderbook.sellOrders,
				std::greater<PriceType>()
			);
	}
	return matchCount;
}

// Templated helper to cancel an order within a given orders map.
bool modify_order(
	std::vector<PriceType>& prices,
	std::vector<VolumeType>& volumes,
	std::vector<std::deque<Order>>& orders,
	IdType order_id,
	QuantityType new_quantity
)
{
	for (auto it = orders.begin(); it != orders.end();)
	{
		auto& ordersAtLevel = *it;
		size_t index = std::distance(orders.begin(), it);
		for (auto orderIt = ordersAtLevel.begin(); orderIt != ordersAtLevel.end();)
		{
			if (orderIt->id == order_id)
			{
				QuantityType prevQuantity = orderIt->quantity;

				if (new_quantity == 0)
				{
					orderIt = ordersAtLevel.erase(orderIt);
					volumes[index] -= prevQuantity;
					break;
				}

				orderIt->quantity = new_quantity;
				volumes[index] += (new_quantity - prevQuantity);
				return true;
			}
			++orderIt;
		}
		if (ordersAtLevel.empty())
		{
			volumes.erase(volumes.begin() + index);
			prices.erase(prices.begin() + index);
			it = orders.erase(it);
		}
		else
		{
			++it;
		}
	}
	return false;
}

void modify_order_by_id(Orderbook& orderbook, IdType order_id, QuantityType new_quantity)
{
	if (modify_order(
			orderbook.buyPrices,
			orderbook.buyVolumes,
			orderbook.buyOrders,
			order_id,
			new_quantity
		))
		return;
	if (modify_order(
			orderbook.sellPrices,
			orderbook.sellVolumes,
			orderbook.sellOrders,
			order_id,
			new_quantity
		))
		return;
}

std::optional<Order> lookup_order(std::vector<std::deque<Order>>& orders, IdType order_id)
{
	for (auto it = orders.begin(); it != orders.end();)
	{
		auto& ordersAtLevel = *it;
		for (auto orderIt = ordersAtLevel.begin(); orderIt != ordersAtLevel.end();)
		{
			if (orderIt->id == order_id)
				return *orderIt;
			++orderIt;
		}
		++it;
	}
	return std::nullopt;
}

uint32_t get_volume_at_level(Orderbook& orderbook, Side side, PriceType price)
{
	if (side == Side::BUY)
	{
		auto it = std::find_if(
			orderbook.buyPrices.begin(),
			orderbook.buyPrices.end(),
			[price](const auto p) { return p == price; }
		);
		return it != orderbook.buyPrices.end()
			? orderbook.buyVolumes[std::distance(orderbook.buyPrices.begin(), it)]
			: 0;
	}
	else if (side == Side::SELL)
	{
		auto it = std::find_if(
			orderbook.sellPrices.begin(),
			orderbook.sellPrices.end(),
			[price](const auto p) { return p == price; }
		);
		return it != orderbook.sellPrices.end()
			? orderbook.sellVolumes[std::distance(orderbook.sellPrices.begin(), it)]
			: 0;
	}
	return 0;
}

// Functions below here don't need to be performant. Just make sure they're
// correct
Order lookup_order_by_id(Orderbook& orderbook, IdType order_id)
{
	auto order1 = lookup_order(orderbook.buyOrders, order_id);
	auto order2 = lookup_order(orderbook.sellOrders, order_id);
	if (order1.has_value())
		return *order1;
	if (order2.has_value())
		return *order2;
	throw std::runtime_error("Order not found");
}

bool order_exists(Orderbook& orderbook, IdType order_id)
{
	auto order1 = lookup_order(orderbook.buyOrders, order_id);
	auto order2 = lookup_order(orderbook.sellOrders, order_id);
	return (order1.has_value() || order2.has_value());
}

Orderbook* create_orderbook() { return new Orderbook; }
