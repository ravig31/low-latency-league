#pragma once

#include <array>
#include <cstdint>
#include <optional>

enum class Side : uint8_t
{
	BUY,
	SELL
};

using IdType = uint32_t;
using PriceType = uint16_t;
using QuantityType = uint16_t;

// You CANNOT change this
struct Order
{
	IdType id; // Unique
	PriceType price;
	QuantityType quantity;
	Side side;
};

constexpr uint16_t MAX_ORDERS = 10'000;
constexpr uint16_t MAX_ORDERS_PER_LEVEL = 496;
constexpr uint16_t MAX_PRICE = 4400;

struct PriceLevel
{
	uint32_t volume = 0;
	uint16_t count = 0;
	std::array<IdType, MAX_ORDERS_PER_LEVEL> orders;
};

// You CAN and SHOULD change this
struct Orderbook {
    std::array<PriceLevel, MAX_PRICE> buyLevels;
    std::array<PriceLevel, MAX_PRICE> sellLevels;
    std::array<std::optional<Order>, MAX_ORDERS> orders;

    Orderbook() : buyLevels{}, sellLevels{} {} 
};

// // You CAN and SHOULD change this
// struct Orderbook
// {
// 	std::vector<std::pair<PriceType, std::pair<VolumeType, std::deque<Order>>>> buyLevels;
// 	std::vector<std::pair<PriceType, std::pair<VolumeType, std::deque<Order>>>> sellLevels;
// };

extern "C"
{

	// Takes in an incoming order, matches it, and returns the number of matches
	// Partial fills are valid

	uint32_t match_order(Orderbook& orderbook, const Order& incoming);

	// Sets the new quantity of an order. If new_quantity==0, removes the order
	void modify_order_by_id(Orderbook& orderbook, IdType order_id, QuantityType new_quantity);

	// Returns total resting volume at a given price point
	uint32_t get_volume_at_level(Orderbook& orderbook, Side side, PriceType price);

	// Performance of these do not matter. They are only used to check correctness
	Order lookup_order_by_id(Orderbook& orderbook, IdType order_id);
	bool order_exists(Orderbook& orderbook, IdType order_id);
	Orderbook* create_orderbook();
}
