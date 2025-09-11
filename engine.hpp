#pragma once

#include "circular_buffer.h"
#include "sorted_array.h"

#include <array>
#include <bitset>
#include <cstdint>

enum class Side : uint8_t { BUY, SELL };

using IdType = uint32_t;
using PriceType = uint16_t;
using LevelPriceType = int16_t;
using QuantityType = uint16_t;
using VolumeType = uint32_t;

static constexpr uint16_t MAX_ORDERS = 10'000;
static constexpr uint16_t MAX_ORDERS_PER_LEVEL = 25;
static constexpr uint16_t MAX_NUM_PRICES = 8192;
static constexpr uint16_t BASE_PRICE = 0;

// You CANNOT change this
struct Order {
    IdType id; // Unique
    PriceType price;
    QuantityType quantity;
    Side side;
};

using PriceLevels = SortedFixedArray<LevelPriceType, MAX_NUM_PRICES>;
using OrderLevels =
    std::array<CircularBuffer<IdType, MAX_ORDERS_PER_LEVEL>, MAX_NUM_PRICES>;
using Volumes = std::array<VolumeType[2], MAX_NUM_PRICES>;

using OrderStore = std::array<Order, MAX_ORDERS>;
using OrderBitSet = std::bitset<MAX_ORDERS>;

// You CAN and SHOULD change this
struct alignas(64) Orderbook {
    alignas(64) SortedFixedArray<LevelPriceType, MAX_NUM_PRICES> buyLevels{};
    alignas(64) std::array<CircularBuffer<IdType, MAX_ORDERS_PER_LEVEL>,
                           MAX_NUM_PRICES> buyIds{};

    alignas(64) SortedFixedArray<LevelPriceType, MAX_NUM_PRICES> sellLevels{};
    alignas(64) std::array<CircularBuffer<IdType, MAX_ORDERS_PER_LEVEL>,
                           MAX_NUM_PRICES> sellIds{};

    alignas(64) std::array<VolumeType[2], MAX_NUM_PRICES> volumes{};

    alignas(64) std::array<Order, MAX_ORDERS> orders{};
    alignas(64) std::bitset<MAX_ORDERS> ordersActive{};
};

extern "C" {
// Takes in an incoming order, matches it, and returns the number of matches
// Partial fills are valid

uint32_t match_order(Orderbook &orderbook, const Order &incoming) noexcept;

// Sets the new quantity of an order. If new_quantity==0, removes the order
void modify_order_by_id(Orderbook &orderbook, IdType order_id,
                        QuantityType new_quantity) noexcept;

// Returns total resting volume at a given price point
uint32_t get_volume_at_level(Orderbook &orderbook, Side side,
                             PriceType price) noexcept;

// Performance of these do not matter. They are only used to check correctness
Order lookup_order_by_id(Orderbook &orderbook, IdType order_id);
bool order_exists(Orderbook &orderbook, IdType order_id);
Orderbook *create_orderbook();
}
