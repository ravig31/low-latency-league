#pragma once

#include "circular_buffer.h"
#include "reverse_array.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <utility>

enum class Side : uint8_t { BUY, SELL };

using IdType = uint32_t;
using PriceType = uint16_t;
using QuantityType = uint16_t;
using VolumeType = uint32_t;

static constexpr uint16_t MAX_ORDERS = 10'000;
static constexpr uint16_t MAX_ORDERS_PER_LEVEL = 25;
static constexpr uint16_t MAX_NUM_PRICES = 8192;

// experimenting with range and size of possible price levels
static constexpr uint16_t BASE_PRICE = 0;

// You CANNOT change this
struct Order {
    IdType id; // Unique
    PriceType price;
    QuantityType quantity;
    Side side;
};

using Volumes = std::array<VolumeType[2], MAX_NUM_PRICES>;
using OrderStore = std::array<Order, MAX_ORDERS>;
using OrderBitSet = std::bitset<MAX_ORDERS>;

struct OBSide {
  private:
    using OrdQueue = CircularBuffer<IdType, MAX_ORDERS_PER_LEVEL>;

    ReverseSortedArray<int16_t, MAX_NUM_PRICES> _prices;
    std::array<OrdQueue, MAX_NUM_PRICES> _orders;

  public:
    __attribute__((always_inline, hot)) inline std::pair<OrdQueue *, PriceType>
    get_best() {
        if (!_prices.empty()) {
            auto best_price = std::abs(_prices.back()) - BASE_PRICE;
            return {&_orders[best_price], best_price};
        }
        return {nullptr, 0};
    }

    __attribute__((always_inline, hot)) inline void remove_best() {
        _prices.pop_back();
    }

    // Keeps buy prices negated in the book and sell prices negated in order
    // for unified comparison logic. Always check if (negged) order price >= best
    // price in book otherwise order cannot be filled
    // e.g. if sell order negged price is -99 (99) and best bid is -100 (100) 
	// order cannot not be filled (-99 < -100).
    // e.g. if buy order comparison price is 100 and best ask is 101
    // cannot be filled (100 < 101)
    __attribute__((always_inline, hot)) inline bool can_fill(Order &order) {
        return !_prices.empty() &&
               (order.side == Side::SELL
                    ? -1 * static_cast<int16_t>(order.price)
                    : static_cast<int16_t>(order.price)) >= _prices.back();
    }

    __attribute__((always_inline, hot)) inline void add_order(Order &order) {
        if (_orders[order.price - BASE_PRICE].push_back(order.id)) {
            _prices.insert(order.side == Side::BUY
                               ? -1 * static_cast<int16_t>(order.price)
                               : static_cast<int16_t>(order.price));
        }
    }
};

// You CAN and SHOULD change this
struct Orderbook {
    alignas(64) OBSide _buy_levels{};
    alignas(64) OBSide _sell_levels{};

    alignas(64) std::array<VolumeType[2], MAX_NUM_PRICES> _volumes{};
    alignas(64) std::array<Order, MAX_ORDERS> _orders{};
    alignas(64) std::bitset<MAX_ORDERS> _orders_active{};
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
