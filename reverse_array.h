#pragma once

#include <array>

template <typename T, std::size_t Size> class ReverseSortedArray {
  private:
    std::array<T, Size> data;
    std::size_t count = 0; // Number of actual elements in the array

  public:
    bool empty() const { return count == 0; }
    bool full() const { return count == Size; }
    std::size_t size() const { return count; }
    std::size_t capacity() const { return Size; }

    inline __attribute__((always_inline, hot)) const T &back() const {
        return data[count - 1];
    }
    // Remove the last element (smallest value)
    inline __attribute__((always_inline, hot)) void pop_back() {
        if (empty()) {
            return;
        }
        --count;
    }

    // Insert a new element, maintaining decreasing sorted order
    // Returns true if inserted, false if array is full
    template <typename U> bool insert(const U &&value) noexcept {
        if (full()) [[unlikely]] {
            return false;
        }
        // Start searching from the back (smallest values) to find insertion
        // point
        std::size_t pos = count;
        while (pos > 0 && value > data[pos - 1]) {
            --pos;
        }

		// move range [pos, end) one position to the right to make space for new element
        if (pos < count) [[likely]] {
            std::move_backward(data.begin() + pos, data.begin() + count,
                               data.begin() + count + 1);
        }
        // Insert the value at the found position
        data[pos] = value;
        ++count;
        return true;
    }
};
