# NUFT Low-latency-league 

My implementation/optimisations for [NUFT's Low Latency League](https://github.com/northwesternfintech/low-latency-league) matching engine competition. Most of the ideas for this came from this great [CppCon video](https://www.youtube.com/watch?v=sX2nF1fW7kI&t=1503s&pp=ygUOY3BwY29uIG9wdGl2ZXLSBwkJ9gkBhyohjO8%3D).

The approach centres around the statistical reality that “hot” price levels form a narrow band. By making that locality assumption explicit, we can trade wide price coverage for consistently low cache miss rates, branch predictability, and allocation-free behavior. All of which dominate latency in a bursty, center of book workload.

### Running the code

Note the benchmark file is compiled only for `x86_64` Linux. In addtion, requires you to have `PAPI` and `perf` installed and available in your path.
```Makefile
make benchmark # run competition benchmark
make test # run tests
```

## Optimisation 1 - Choice of Data Structure
The following intermediate approaches were explored (not all appear in the current code – they are design iterations):

1. std::map / std::list (classical textbook structures)
   - Each node allocated separately → pointer chasing & poor spatial locality
   - Tree rebalancing / list traversal overhead
   - Discarded early

2. Vector of price levels with full order containers:  
   `std::vector<std::pair<PriceType, std::vector<Order>>>`
   - Pros: Contiguous price levels, simple insertion
   - Cons:
     - Adding a new middle price may shift many elements
     - Each inner orders vector can cause separate allocations
     - Volume per level requires summing (O(n_orders_at_level))
     - Price iteration pulls entire (price + orders) tuples → cache pollution (AoS layout)
     - Modifying/cancelling specific orders often degenerates to linear scans

3. Variant with cached volume + deque:
   `std::vector<std::pair<PriceType, std::pair<VolumeType, std::deque<Order>>>>`
   - Pros: O(1) volume lookup, O(1) push/pop ends with deque
   - Cons: Deque blocks reduce locality; still AoS; per-level structure still large → unnecessary cache fills
   - Still multiple dynamic allocations

4. Final design (current code)
   - Replace dynamic structures with fixed‑capacity, cache‑friendly, SoA style layout

Trade-off: the real downside with a fixed range is the price coverage, however for the sake of the benchmark range it works.

## Optimisation 2 - Unified Matching via Price Sign Normalisation
When inserting:
```cpp
_prices.insert(order.side == Side::BUY
    ? -static_cast<int16_t>(order.price)
    :  static_cast<int16_t>(order.price));
```
With the array sorted in strictly decreasing order:
- Best ask (lowest positive) and best bid (most negative) **both end up at the back**
- Matching logic can treat "best" uniformly via `abs(_prices.back())`
- Eliminates side‑specific comparison branches in hot path
- Fits safely in `int16_t` given configured price domain (ensure benchmark constraints keep prices < 32768)

Trade‑off: The performance increase is not all that signigicant, and there is a slight readability cost with this approach. Though, for the sake of pure optimisation I decided to include it anyway.

## Current Implementation


Core components (SoA split):
- Price ladder per side: `ReverseSortedArray<int16_t, MAX_NUM_PRICES>`
  - Stores negated BUY prices, positive SELL prices
  - Kept in (strict) decreasing order
  - Access to best price uses `back()` (smallest stored value) for both sides
    - Rationale:
      - For BUY: more competitive = higher numeric price = more negative stored value (e.g. -101 < -100); smallest (most negative) = highest real price
      - For SELL: more competitive = lower price; with descending order, the lowest positive ends up at the back
- Per‑price FIFO order queues: `std::array<CircularBuffer<IdType>, MAX_NUM_PRICES>`
  - Fast append at tail / consume from head
  - Stores only order IDs (not full structs) → small, cache friendly
- Global order store: `std::array<Order, MAX_ORDERS>`
  - Dense indexable storage
- Active mask: `std::bitset<MAX_ORDERS>`
  - Lazy cancellation: mark inactive, skip during matching
- Per‑price, per‑side volume: `std::array<VolumeType[2], MAX_NUM_PRICES>`
  - O(1) volume retrieval

Why the `ReverseSortedArray`? 


- Fixed capacity avoids allocator calls
- Insertion from the back **scans only as far as needed**, then uses `std::move_backward` on a tight contiguous range
- Using `_prices.back()` as the unified "best" accessor avoids branching for whether we are dealing with BUY or SELL


## Why this approach?

Like mentioned, real limit order books are not uniformly populated across the theoretical price range. Resting liquidity tends to bunch in a relatively narrow band around the prevailing market price. Far‑away price levels are either empty or thin. This empirical skew lets us bias the in‑memory layout toward:

1. Fast-path locality:  
   - The most frequently touched data (best level price encoding, per‑price FIFO of order IDs, per‑level volume) lives in a small, tightly packed, **contiguous set of cache lines**.
   - Because new aggressive orders almost always interact with the current best (or a few adjacent levels), the **working set stays hot**

2. Cheap sparse coverage:  
   - A fixed-capacity array for all possible prices looks wasteful in worst-case theory, but **in practice the inactive tail is never touched**, so it's really just reserved address space.
   - No dynamic metadata or node allocations are paid for empty regions, avoiding pointer chasing entirely.

5. Sign-normalised unified best access:  
   - Converging both BUY and SELL best-level discovery to a single `back()` access **removes a side branch exactly where latency matters most**.
   - The **clustered nature** of activity around the top-of-book **amplifies the benefit**.

## Limitations & Future Improvements
- Price range fixed at compile time (MAX_NUM_PRICES). A sparse market with very wide price dispersion would waste memory.
	- Circular buffer capacity (MAX_ORDERS_PER_LEVEL) is a hard cap
	- Needs validation against benchmark constraints.
- **Negated price trick lowers readability**
	- Could wrap in a strong type for clarity without perf loss (if inlined).
- Matching still performs linear consumption within a price level
	- Could consider **SIMD aggregation** for volume pre-checks (if justified).
- Potential improvements: 
	- Adopt `std::pmr::monotonic_buffer_resource` for better tradeoff between price coverage and performance
	- No batching / **vectorization** of order processing yet.


