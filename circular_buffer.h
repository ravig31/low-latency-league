
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

template <typename T, size_t Capacity> class CircularBuffer
{
	static_assert(Capacity > 0, "Invalid capacity");

  private:
	std::array<T, Capacity> buffer_{};
	uint64_t head = 0;
	uint64_t tail = 0;

  public:
	inline __attribute__((always_inline, hot)) T front() const { return buffer_[tail]; }
	inline __attribute__((always_inline, hot)) bool empty() const { return tail == head; }
	inline __attribute__((always_inline, hot)) bool full() const { return head == Capacity; }
	inline __attribute__((always_inline, hot)) bool push_back(T item)
	{
		if (full())
		{
			return false;
		}
		buffer_[head++] = item;
		return true;
	}

	inline __attribute__((always_inline, hot)) void pop_front() { ++tail; }
};
