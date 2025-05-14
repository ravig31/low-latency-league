#pragma once

#include <array>
#include <algorithm>
#include <stdexcept>
#include <optional>


template<typename T, std::size_t Size>
class SortedFixedArray {
private:
    std::array<T, Size> data;
    std::size_t count = 0;  // Number of actual elements in the array

public:
    // Constructor
    SortedFixedArray() = default;

    [[nodiscard]] bool empty() const {
        return count == 0;
    }

    [[nodiscard]] bool full() const {
        return count == Size;
    }

    [[nodiscard]] std::size_t size() const {
        return count;
    }

    [[nodiscard]] std::size_t capacity() const {
        return Size;
    }

    const T& at(std::size_t index) const {
        if (index >= count) {
            throw std::out_of_range("Index out of range");
        }
        return data[index];
    }

    T& at(std::size_t index) {
        if (index >= count) {
            throw std::out_of_range("Index out of range");
        }
        return data[index];
    }

    const T& operator[](std::size_t index) const {
        return data[index];
    }

    T& operator[](std::size_t index) {
        return data[index];
    }

    const T& front() const {
        if (empty()) {
            throw std::out_of_range("Container is empty");
        }
        return data[0];
    }

    T& front() {
        if (empty()) {
            throw std::out_of_range("Container is empty");
        }
        return data[0];
    }

    const T& back() const {
        if (empty()) {
            throw std::out_of_range("Container is empty");
        }
        return data[count - 1];
    }

    T& back() {
        if (empty()) {
            throw std::out_of_range("Container is empty");
        }
        return data[count - 1];
    }

    // Remove the last element (smallest value)
    void pop_back() {
        if (empty()) {
            throw std::out_of_range("Cannot pop from empty container");
        }
        --count;
    }

    auto begin() { return data.begin(); }
    auto end() { return data.begin() + count; }

    // Insert a new element, maintaining decreasing sorted order
    // Returns true if inserted, false if array is full and value is too small
    inline __attribute__((always_inline, hot)) bool insert(const T& value) {
        if (full()) {
            return false;
        }
        // Start searching from the back (smallest values) to find insertion point
        std::size_t pos = count;
        while (pos > 0 && value > data[pos - 1]) {
            --pos;
        }
        if (pos < count) {
            std::move_backward(data.begin() + pos, data.begin() + count, data.begin() + count + 1);
        }
        // Insert the value at the found position
        data[pos] = value;
        ++count;
        return true;
    }
    
    
    inline __attribute__((always_inline, hot)) bool erase(const T& value) {
        for (std::size_t i = count; i-- > 0;) {
            if (data[i] == value) {
                std::move(data.begin() + i + 1, data.begin() + count, data.begin() + i);
                --count;
                return true;
            }
            // Early termination: if we've gone past where the value could be
            // (due to decreasing sort order)
            if (data[i] > value) {
                break;
            }
        }
        return false;
    }

    void clear() {
        count = 0;
    }

};

