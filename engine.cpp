#include "engine.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

inline __attribute__((always_inline, hot)) bool add_order(
	LevelPriceType modPrice,
	Order& order,
	PriceLevels& priceLevels,
	OrderLevels& orderLevels,
	Volumes& volumes
) noexcept
{
	if (orderLevels[order.price - BASE_PRICE].push_back(order.id))
	{
		priceLevels.insert(modPrice);
		volumes[order.price - BASE_PRICE][static_cast<size_t>(order.side)] += order.quantity;
		return true;
	}
	return false;
}

inline __attribute__((always_inline, hot)) uint32_t process_orders(
	LevelPriceType price,
	Order& order,
	PriceLevels& priceLevels,
	OrderLevels& orderLevels,
	Volumes& volumes,
	OrderStore& orders,
	OrderBitSet& ordersActive
) noexcept
{
	uint32_t matchCount = 0;

	while (order.quantity > 0)
	{

		// if no prices to match or cannot match at best price
		if (priceLevels.empty() || price < priceLevels.back()) [[unlikely]]
			break;

		size_t priceIdx = abs(priceLevels.back()) - BASE_PRICE;
		VolumeType& volAtLevel = volumes[priceIdx][!static_cast<size_t>(order.side)];
		auto& ordersAtLevel = orderLevels[priceIdx];

		while (!ordersAtLevel.empty() && order.quantity > 0)
		{
			IdType counterId = ordersAtLevel.front();
			if (!ordersActive[counterId])
			{
				ordersAtLevel.pop_front();
				if (orderLevels.empty())
				{
					priceLevels.erase(price);
					break;
				}
				continue;
			}
			auto& counterOrder = orders[counterId];

			QuantityType trade = std::min(order.quantity, counterOrder.quantity);
			order.quantity -= trade;
			counterOrder.quantity -= trade;
			volAtLevel -= trade; // Decrement total volume at level

			if (counterOrder.quantity == 0) [[likely]]
			{
				ordersActive.reset(counterId);
				ordersAtLevel.pop_front();
			}
			++matchCount;
		}

		if (ordersAtLevel.empty()) [[unlikely]]
		{
			priceLevels.erase(priceLevels.back());
		}
	}
	return matchCount;
}

uint32_t match_order(Orderbook& orderbook, const Order& incoming) noexcept
{
	uint32_t matchCount = 0;
	Order order = incoming;
	LevelPriceType modifiedPrice = static_cast<int16_t>(order.price);

	matchCount = process_orders(
		order.side == Side::SELL ? -modifiedPrice : modifiedPrice,
		order,
		order.side == Side::SELL ? orderbook.buyLevels : orderbook.sellLevels,
		order.side == Side::SELL ? orderbook.buyIds : orderbook.sellIds,
		orderbook.volumes,
		orderbook.orders,
		orderbook.ordersActive
	);
	if (order.quantity > 0) [[unlikely]]
	{
		add_order(
			order.side == Side::BUY ? -modifiedPrice : modifiedPrice,
			order,
			order.side == Side::BUY ? orderbook.buyLevels : orderbook.sellLevels,
			order.side == Side::BUY ? orderbook.buyIds : orderbook.sellIds,
			orderbook.volumes
		);
		orderbook.ordersActive.set(order.id);
		orderbook.orders[order.id] = order;
	}

	return matchCount;
}

void modify_order_by_id(Orderbook& orderbook, IdType order_id, QuantityType new_quantity) noexcept
{
	if (!orderbook.ordersActive[order_id]) [[unlikely]]
	{
		return;
	}
	auto& order = orderbook.orders[order_id];
	orderbook.volumes[order.price][static_cast<size_t>(order.side)] +=
		(new_quantity - order.quantity);

	if (new_quantity == 0) [[likely]]
	{
		orderbook.ordersActive.reset(order_id);
	}
	else [[unlikely]]
		order.quantity = new_quantity; // Update quantity in orders array
}

uint32_t get_volume_at_level(Orderbook& orderbook, Side side, PriceType price) noexcept
{
	return orderbook.volumes[price][static_cast<size_t>(side)];
}

// Functions below here don't need to be performant. Just make sure they're
// correct
Order lookup_order_by_id(Orderbook& orderbook, IdType order_id)
{
	if (!orderbook.ordersActive[order_id])
		throw std::runtime_error("Order not found");

	return orderbook.orders[order_id];
}

bool order_exists(Orderbook& orderbook, IdType order_id)
{
	return orderbook.ordersActive[order_id];
}

Orderbook* create_orderbook() { return new Orderbook; }
