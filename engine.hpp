#pragma once

#include "circular_buffer.h"

#include <array>
#include <bitset>
#include <functional>
#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>
#include <vector>

enum class Side : uint8_t
{
	BUY,
	SELL
};

using IdType = uint32_t;
using PriceType = uint16_t;
using LevelPriceType = int16_t;
using QuantityType = uint16_t;
using VolumeType = uint32_t;

constexpr uint16_t MAX_ORDERS = 10'000;
constexpr uint16_t MAX_ORDERS_PER_LEVEL = 256;
// You CANNOT change this
struct Order
{
	IdType id; // Unique
	PriceType price;
	QuantityType quantity;
	Side side;
};

struct PriceLevel
{

	uint32_t volume = 0;
	CircularBuffer<IdType> orders;

	inline void add_order(IdType order_id, QuantityType quantity)
	{
		volume += quantity;
		orders.push_back(order_id);
	}

	inline void fill_front_order(QuantityType quantity)
	{
		volume -= quantity;
		orders.pop_front();
	}

	inline void find_and_remove_order(IdType order_id)
	{
		auto it = std::find(orders.begin(), orders.end(), order_id);
		if (it == orders.end())
			throw std::runtime_error("Order id does not exist in level");

		orders.erase(it);
	}

	PriceLevel()
		: volume(0)
		, orders(MAX_ORDERS_PER_LEVEL)
	{
	}

	PriceLevel(IdType order_id, QuantityType quantity)
		: volume(0)
		, orders(MAX_ORDERS_PER_LEVEL)
	{
		add_order(order_id, quantity);
	}
};

using Levels = std::pmr::vector<std::pair<LevelPriceType, PriceLevel>>;
using LevelSpan = std::span<std::pair<PriceType, PriceLevel>>;
using Orders = std::array<Order, MAX_ORDERS>;
using OrdersActive = std::bitset<MAX_ORDERS>;
using Cond = std::function<bool(LevelPriceType, LevelPriceType)>;

// You CAN and SHOULD change this
struct Orderbook
{
	alignas(64) char buffer[512 * 1024];
	std::pmr::monotonic_buffer_resource resource{buffer, sizeof(buffer)};

	std::pmr::vector<std::pair<LevelPriceType, PriceLevel>> buyLevels{&resource};
	std::pmr::vector<std::pair<LevelPriceType, PriceLevel>> sellLevels{&resource};

	std::array<Order, MAX_ORDERS> orders;
	std::bitset<MAX_ORDERS> ordersActive;

	Orderbook()
		: buyLevels()
		, sellLevels{}
	{
		buyLevels.reserve(256);
		sellLevels.reserve(256);
	}
};

extern "C"
{

	// Takes in an incoming order, matches it, and returns the number of matches
	// Partial fills are valid

	uint32_t match_order(Orderbook& orderbook, const Order& incoming) noexcept;

	// Sets the new quantity of an order. If new_quantity==0, removes the order
	void modify_order_by_id(Orderbook& orderbook, IdType order_id, QuantityType new_quantity) noexcept;

	// Returns total resting volume at a given price point
	uint32_t get_volume_at_level(Orderbook& orderbook, Side side, PriceType price) noexcept;

	// Performance of these do not matter. They are only used to check correctness
	Order lookup_order_by_id(Orderbook& orderbook, IdType order_id);
	bool order_exists(Orderbook& orderbook, IdType order_id);
	Orderbook* create_orderbook();
}
