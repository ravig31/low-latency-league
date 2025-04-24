 
1. Simple thing: multiply all BID prices by -1. After this you can be sure that a better price is lower price. This removes a branch misprediction problem which fails in 50% of cases. Or multiply asks by -1, it doesn't matter. 

2. Prices are aligned to price step. PriceInSteps=price/priceStep is an int32 variable. One can use it as an index in an array of price levels, so we don't need to store it at all. We need to store only volume in this array. We get even better cache locality and O(1) complexity for all operations. Improvement: limit the size of an order book to 128 elements and use a ring buffer array with mod 128 arithmetics, which can be implemented as bit mask x|127, which works for both positive and negative numbers.


