#include "engine.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like
inline __attribute__((always_inline, hot)) uint32_t process_orders(
    Order &order, OBSide &x_levels, OBSide &s_levels, Volumes &volumes,
    OrderStore &orders, OrderBitSet &_orders_active) noexcept {

    uint32_t match_count = 0;
    while (order.quantity > 0) {

        if (!x_levels.can_fill(order)) [[unlikely]]
            break;

        auto [orders_at_level, best_price] = x_levels.get_best();

        VolumeType &vol_at_level =
            volumes[best_price][!static_cast<size_t>(order.side)];

        while (!orders_at_level->empty() && order.quantity > 0) {
            IdType counter_order_id = orders_at_level->front();

            // order was cancelled previously, remove and continue
            if (!_orders_active[counter_order_id]) [[unlikely]] {
                orders_at_level->pop_front();
                if (orders_at_level->empty()) {
                    x_levels.remove_best();
                    break;
                }
                continue;
            }
            auto &counter_order = orders[counter_order_id];

            QuantityType trade =
                std::min(order.quantity, counter_order.quantity);
            order.quantity -= trade;
            counter_order.quantity -= trade;
            vol_at_level -= trade; // Decrement total volume at level

            if (counter_order.quantity == 0) [[likely]] {
                _orders_active.reset(counter_order_id);
                orders_at_level->pop_front();
            }
            ++match_count;
        }

        if (orders_at_level->empty()) [[unlikely]] {
            x_levels.remove_best();
        }
    }

    // Add order to corresponding side if partially filled
    if (order.quantity > 0) [[unlikely]] {
        s_levels.add_order(order);
        volumes[order.price - BASE_PRICE][static_cast<size_t>(order.side)] +=
            order.quantity;
        _orders_active.set(order.id);
        orders[order.id] = order;
    };
    return match_count;
};

uint32_t match_order(Orderbook &orderbook, const Order &incoming) noexcept {
    uint32_t match_count = 0;
    Order order = incoming;
    const bool isSell = static_cast<bool>(order.side);

    match_count = process_orders(
        order, isSell ? orderbook._buy_levels : orderbook._sell_levels,
        isSell ? orderbook._sell_levels : orderbook._buy_levels,
        orderbook._volumes, orderbook._orders, orderbook._orders_active);

    return match_count;
}

void modify_order_by_id(Orderbook &orderbook, IdType order_id,
                        QuantityType new_quantity) noexcept {
    if (!orderbook._orders_active[order_id]) [[unlikely]] {
        return;
    }

    auto &order = orderbook._orders[order_id];
    orderbook._volumes[order.price][static_cast<uint8_t>(order.side)] +=
        (new_quantity - order.quantity);

    if (new_quantity == 0) [[likely]] {
        orderbook._orders_active.reset(order_id);
    } else [[unlikely]]
        order.quantity = new_quantity; // Update quantity in orders array
}

uint32_t get_volume_at_level(Orderbook &orderbook, Side side,
                             PriceType price) noexcept {
    return orderbook._volumes[price][static_cast<uint8_t>(side)];
}

// Functions below here don't need to be performant. Just make sure they're
// correct
Order lookup_order_by_id(Orderbook &orderbook, IdType order_id) {
    if (!orderbook._orders_active[order_id])
        throw std::runtime_error("Order not found");

    return orderbook._orders[order_id];
}

bool order_exists(Orderbook &orderbook, IdType order_id) {
    return orderbook._orders_active[order_id];
}

Orderbook *create_orderbook() { return new Orderbook; }
