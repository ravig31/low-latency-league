#include "engine.hpp"
#include <algorithm>
#include <functional>
#include <optional>
#include <stdexcept>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.

template <typename OrderList, typename Condition>
void add_order(Order& order, OrderList& ordersList, Condition cond)
{
	if (ordersList.empty())
	{
		ordersList.push_back({ order.price, { order.quantity, std::deque<Order>{ order } } });
		return;
	}

	auto rit = ordersList.rbegin();
	while (rit != ordersList.rend() && cond(order.price, rit->first))
	{
		++rit;
	}

	if (rit != ordersList.rend() && rit->first == order.price)
	{
		rit->second.second.push_back(order);
		rit->second.first += order.quantity;
	}
	else
	{
		ordersList.insert(
			rit.base(),
			{ order.price, { order.quantity, std::deque<Order>{ order } } }
		);
	}
}

template <typename OrderList, typename Condition>
uint32_t process_orders(Order& order, OrderList& ordersList, Condition cond)
{
	uint32_t matchCount = 0;
	auto it = ordersList.rbegin();
	while (it != ordersList.rend() && order.quantity > 0 &&
		   (it->first == order.price || cond(order.price, it->first)))
	{
		auto& ordersAtLevel = it->second.second;
		while (!ordersAtLevel.empty() && order.quantity > 0)
		{
			QuantityType trade = std::min(order.quantity, ordersAtLevel.front().quantity);
			order.quantity -= trade;
			ordersAtLevel.front().quantity -= trade;
			it->second.first -= trade; // Decrement total volume at level
			if (ordersAtLevel.front().quantity == 0)
				ordersAtLevel.pop_front();
			++matchCount;
		}
		it = ordersAtLevel.empty() ? std::make_reverse_iterator(ordersList.erase(--it.base()))
								   : it++;
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
		matchCount = process_orders(order, orderbook.sellOrders, std::greater<>());
		if (order.quantity > 0)
			add_order(order, orderbook.buyOrders, std::less<PriceType>());
	}
	else
	{ // Side::SELL
		// For a SELL, match with buy orders priced at or above the order's price.
		matchCount = process_orders(order, orderbook.buyOrders, std::less<>());
		if (order.quantity > 0)
			add_order(order, orderbook.sellOrders, std::greater<PriceType>());
	}
	return matchCount;
}

// Templated helper to cancel an order within a given orders map.
template <typename OrderList>
bool modify_order_in_map(OrderList& ordersList, IdType order_id, QuantityType new_quantity)
{
	for (auto it = ordersList.begin(); it != ordersList.end();)
	{
		auto& ordersAtLevel = it->second.second;
		auto& volumeAtLevel = it->second.first;
		for (auto orderIt = ordersAtLevel.begin(); orderIt != ordersAtLevel.end();)
		{
			if (orderIt->id == order_id)
			{
				QuantityType prevQuantity = orderIt->quantity;

				if (new_quantity == 0)
				{
					orderIt = ordersAtLevel.erase(orderIt);
					volumeAtLevel -= prevQuantity;
					break;
				}

				orderIt->quantity = new_quantity;
				volumeAtLevel += (new_quantity - prevQuantity);
				return true;
			}
			++orderIt;
		}
		it = ordersAtLevel.empty() ? ordersList.erase(it) : ++it;
	}
	return false;
}

void modify_order_by_id(Orderbook& orderbook, IdType order_id, QuantityType new_quantity)
{
	if (modify_order_in_map(orderbook.buyOrders, order_id, new_quantity))
		return;
	if (modify_order_in_map(orderbook.sellOrders, order_id, new_quantity))
		return;
}

template <typename OrderList>
std::optional<Order> lookup_order_in_map(OrderList& ordersList, IdType order_id)
{
	for (auto it = ordersList.begin(); it != ordersList.end();)
	{
		auto& ordersAtLevel = it->second.second;
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
			orderbook.buyOrders.begin(),
			orderbook.buyOrders.end(),
			[price](const auto& p) { return p.first == price; }
		);
		return it != orderbook.buyOrders.end() ? it->second.first
											   : 0; 
	}
	else if (side == Side::SELL)
	{
		auto it = std::find_if(
			orderbook.sellOrders.begin(),
			orderbook.sellOrders.end(),
			[price](const auto& p) { return p.first == price; }
		);
		return it != orderbook.sellOrders.end() ? it->second.first
												: 0;
	}
	return 0;
}

// Functions below here don't need to be performant. Just make sure they're
// correct
Order lookup_order_by_id(Orderbook& orderbook, IdType order_id)
{
	auto order1 = lookup_order_in_map(orderbook.buyOrders, order_id);
	auto order2 = lookup_order_in_map(orderbook.sellOrders, order_id);
	if (order1.has_value())
		return *order1;
	if (order2.has_value())
		return *order2;
	throw std::runtime_error("Order not found");
}

bool order_exists(Orderbook& orderbook, IdType order_id)
{
	auto order1 = lookup_order_in_map(orderbook.buyOrders, order_id);
	auto order2 = lookup_order_in_map(orderbook.sellOrders, order_id);
	return (order1.has_value() || order2.has_value());
}

Orderbook* create_orderbook() { return new Orderbook; }
