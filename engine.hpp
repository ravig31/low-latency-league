#pragma once

#include "circular_buffer.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>

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
constexpr uint16_t MAX_ORDERS_PER_LEVEL = 512;
constexpr uint16_t BUFFER_SIZE = 256;
const uint16_t BUFFER_MASK = BUFFER_SIZE - 1; // 127 (0x7F)
constexpr PriceType PROACTIVE_CENTER_LEEWAY = BUFFER_SIZE / 4; // How far from true center is acceptable
constexpr PriceType MIN_PRICE_FOR_NON_ZERO_BASE = BUFFER_SIZE / 2;


struct PriceLevel
{

	uint32_t volume = 0;
    CircularBuffer<IdType> orders;

    inline uint16_t count() { return orders.size(); }

    inline void add_order(Order& order);

    inline void fill_front_order(QuantityType quantity);

    inline void find_and_remove_order(Order& order);

	PriceLevel() : orders(MAX_ORDERS_PER_LEVEL) {}
};

// You CAN and SHOULD change this
struct Orderbook
{
	std::array<PriceLevel, BUFFER_SIZE> buyLevels;
	std::array<PriceLevel, BUFFER_SIZE> sellLevels;


	PriceType baseBuyPrice = 0;
	PriceType baseSellPrice = 0;

	std::map<PriceType, PriceLevel, std::greater<PriceType>> buyOutliers;
	std::map<PriceType, PriceLevel> sellOutliers;

	std::unordered_map<PriceType, int> buycounts;
	std::unordered_map<PriceType, int> sellcounts;

	std::array<std::optional<Order>, MAX_ORDERS> orders;
	Orderbook()
		: buyLevels{}
		, sellLevels{}
	{
	}

    int activeLevels(Side side){
        int res = 0;
        for (auto& level: side == Side::BUY ? buyLevels: sellLevels ){
            if (level.volume > 0){
                res++;
            }
        }
        return res;
    }
	void outputCounts(){
		std::cout << "Orderbook destructor called. Printing 'counts' map contents:" << std::endl;

		for (const auto& pair : buycounts) {
			// pair.first is the PriceType (key)
			// pair.second is the int (value)
			std::cout << "  Buy Price Level (Key): " << pair.first
						<< ", Count (Value): " << pair.second << std::endl;
		}

		for (const auto& pair : sellcounts) {
			// pair.first is the PriceType (key)
			// pair.second is the int (value)
			std::cout << "  Sell Price Level (Key): " << pair.first
						<< ", Count (Value): " << pair.second << std::endl;
		}
        
        std::cout << "Finished printing 'counts' map." << std::endl;

		std::cout.flush();
	}


};

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
