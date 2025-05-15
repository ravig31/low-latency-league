#include "engine.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// inline __attribute__((always_inline, hot))
// bool add_order(
// 	LevelPriceType modPrice,
// 	Order& order,
// 	PriceLevels& priceLevels,
// 	OrderLevels& orderLevels,
// 	Volumes& volumes
// ) noexcept
// {

// 	return false;
// }

inline __attribute__((always_inline, hot)) uint32_t process_orders(
	LevelPriceType compPrice,
	Order& order,
	PriceLevels& mPriceLevels,
	OrderLevels& mOrderLevels,
	Volumes& volumes,
	OrderStore& orders,
	OrderBitSet& ordersActive,
	PriceLevels& sPriceLevels,
	OrderLevels& sOrderLevels
) noexcept
{
	uint32_t matchCount = 0;

	while (order.quantity > 0)
	{

		// if no prices to match or cannot match at best price
		if (mPriceLevels.empty() || compPrice < mPriceLevels.back()) [[unlikely]]
			break;

		size_t priceIdx = abs(mPriceLevels.back()) - BASE_PRICE;
		VolumeType& volAtLevel = volumes[priceIdx][!static_cast<size_t>(order.side)];
		auto& ordersAtLevel = mOrderLevels[priceIdx];

		while (!ordersAtLevel.empty() && order.quantity > 0)
		{
			IdType counterId = ordersAtLevel.front();
			if (!ordersActive[counterId]) [[unlikely]]
			{
				ordersAtLevel.pop_front();
				if (mOrderLevels.empty())
				{
					mPriceLevels.pop_back();
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
			mPriceLevels.pop_back();
		}
	}

	// Add order
	if (order.quantity > 0) [[unlikely]]
	{
		if (sOrderLevels[order.price - BASE_PRICE].push_back(order.id))
		{
			sPriceLevels.insert(-compPrice);
			volumes[order.price - BASE_PRICE][static_cast<size_t>(order.side)] += order.quantity;
			ordersActive.set(order.id);
			orders[order.id] = order;
		};
	};
	return matchCount;
}

uint32_t match_order(Orderbook& orderbook, const Order& incoming) noexcept
{
	uint32_t matchCount = 0;
	Order order = incoming;
	const bool isSell = static_cast<bool>(order.side);
	LevelPriceType modifiedPrice = static_cast<int16_t>(order.price);

	matchCount = process_orders(
		isSell ? -modifiedPrice : modifiedPrice,
		order,
		isSell ? orderbook.buyLevels : orderbook.sellLevels,
		isSell ? orderbook.buyIds : orderbook.sellIds,
		orderbook.volumes,
		orderbook.orders,
		orderbook.ordersActive,
		isSell ? orderbook.sellLevels : orderbook.buyLevels,
		isSell ? orderbook.sellIds : orderbook.buyIds
	);

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

	// orderbook.ordersActive[order_id] = new_quantity == 0 ? false : true;
	// order.quantity = new_quantity == 0 ? order.quantity : new_quantity;
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
