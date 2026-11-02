// File purpose: Supplies the custom containers used by the native DSA algorithms.
// Flow:
//   Array provides owned growable storage -> HashMap and UniqueSet build lookup
//   and uniqueness on top -> Stack and BinaryHeap support graph traversals.
// Template implementations stay here so each user can instantiate them.

#ifndef DSA_CONTAINERS_H
#define DSA_CONTAINERS_H

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

using namespace std;

namespace dsa {

// A small growable array for the DSA code, with storage owned by this class.
template <typename T>
class Array {
public:
    using iterator = T*;
    using const_iterator = const T*;

    // Starts empty; the first insertion allocates the backing array.
    Array() = default;

    // Copies the supplied values into the array in their original order.
    Array(initializer_list<T> values) {
        reserve(values.size());
        for (const T& value : values) {
            push_back(value);
        }
    }

    // Makes an independent copy so the two arrays can grow separately.
    Array(const Array& other) {
        reserve(other.size_);
        for (size_t i = 0; i < other.size_; ++i) {
            push_back(other.data_[i]);
        }
    }

    // Takes ownership of another array's allocation without copying values.
    Array(Array&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    // Releases the one allocation owned by this array.
    ~Array() {
        delete[] data_;
    }

    // Uses copy-and-swap so self-assignment and allocation failures are safe.
    Array& operator=(const Array& other) {
        if (this != &other) {
            Array copy(other);
            swap(copy);
        }
        return *this;
    }

    // Gives this array the other array's allocation and empties the source.
    Array& operator=(Array&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    // Exchanges storage and size without moving each stored element.
    void swap(Array& other) noexcept {
        T* data = data_;
        data_ = other.data_;
        other.data_ = data;
        size_t size = size_;
        size_ = other.size_;
        other.size_ = size;
        size_t capacity = capacity_;
        capacity_ = other.capacity_;
        other.capacity_ = capacity;
    }

    // Reports the number of live values, reserved slots, and emptiness.
    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }

    // Exposes pointer iterators so normal loops can walk the stored values.
    iterator begin() { return data_; }
    const_iterator begin() const { return data_; }
    const_iterator cbegin() const { return data_; }
    iterator end() { return data_ == nullptr ? nullptr : data_ + size_; }
    const_iterator end() const {
        return data_ == nullptr ? nullptr : data_ + size_;
    }
    const_iterator cend() const { return end(); }
    // Exposes the owned allocation for APIs that need contiguous values.
    T* data() { return data_; }
    const T* data() const { return data_; }

    // Provides unchecked indexing and checked lookup when bounds matter.
    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }
    T& at(size_t index) {
        if (index >= size_) {
            throw out_of_range("Array index is out of range.");
        }
        return data_[index];
    }
    const T& at(size_t index) const {
        if (index >= size_) {
            throw out_of_range("Array index is out of range.");
        }
        return data_[index];
    }
    // Returns the first or last value; callers must ensure the array is not empty.
    T& front() { return data_[0]; }
    const T& front() const { return data_[0]; }
    T& back() { return data_[size_ - 1]; }
    const T& back() const { return data_[size_ - 1]; }

    // Grows geometrically to avoid allocating on every append.
    void reserve(size_t requested) {
        if (requested <= capacity_) {
            return;
        }
        size_t nextCapacity = capacity_ == 0 ? 4 : capacity_;
        while (nextCapacity < requested) {
            nextCapacity *= 2;
        }
        T* next = new T[nextCapacity];
        for (size_t i = 0; i < size_; ++i) {
            next[i] = move(data_[i]);
        }
        delete[] data_;
        data_ = next;
        capacity_ = nextCapacity;
    }

    // Appends either a copy or a moved value to the next free slot.
    void push_back(const T& value) {
        reserve(size_ + 1);
        data_[size_++] = value;
    }

    // Appends a value by taking over its existing resources when possible.
    void push_back(T&& value) {
        reserve(size_ + 1);
        data_[size_++] = move(value);
    }

    // Builds a value in the next slot and returns it for immediate setup.
    template <typename... Args>
    T& emplace_back(Args&&... args) {
        reserve(size_ + 1);
        data_[size_] = T(forward<Args>(args)...);
        return data_[size_++];
    }

    // Clears live values while keeping the allocation ready for reuse.
    void clear() {
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = T{};
        }
        size_ = 0;
    }

    // Removes one value and shifts the later values toward the front.
    iterator erase(const_iterator position) {
        return erase(position, position + 1);
    }

    // Removes a half-open range and shifts later values into the gap.
    iterator erase(const_iterator first, const_iterator last) {
        const size_t start = static_cast<size_t>(first - data_);
        const size_t finish = static_cast<size_t>(last - data_);
        const size_t removed = finish - start;
        for (size_t i = start; i + removed < size_; ++i) {
            data_[i] = move(data_[i + removed]);
        }
        for (size_t i = size_ - removed; i < size_; ++i) {
            data_[i] = T{};
        }
        size_ -= removed;
        return data_ + start;
    }

    // Inserts a copied range, first buffering it in case the source overlaps.
    template <typename InputIterator>
    iterator insert(const_iterator position, InputIterator first,
                    InputIterator last) {
        Array<T> values;
        for (; first != last; ++first) {
            values.push_back(*first);
        }
        const size_t index =
            data_ == nullptr ? 0 : static_cast<size_t>(position - data_);
        reserve(size_ + values.size_);
        for (size_t i = size_; i > index; --i) {
            data_[i + values.size_ - 1] = move(data_[i - 1]);
        }
        for (size_t i = 0; i < values.size_; ++i) {
            data_[index + i] = move(values[i]);
        }
        size_ += values.size_;
        return data_ + index;
    }

private:
    T* data_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;
};

// Converts supported key types into stable bucket indexes for HashMap.
template <typename K>
struct HashCode {
    // Converts numeric-like keys directly to the bucket hash used here.
    size_t operator()(const K& key) const {
        return static_cast<size_t>(key);
    }
};

template <>
struct HashCode<string> {
    // Mixes each character so similar labels usually land in different buckets.
    size_t operator()(const string& value) const {
        size_t hash = 1469598103934665603ULL;
        for (unsigned char character : value) {
            hash ^= character;
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

template <typename T>
struct HashCode<shared_ptr<T>> {
    // Shared graph objects are keyed by identity, not by their field values.
    size_t operator()(const shared_ptr<T>& value) const {
        return reinterpret_cast<size_t>(value.get());
    }
};

// An open-addressed hash table that keeps entries in an owned dynamic array.
template <typename K, typename V>
class HashMap {
public:
    // Mirrors the key/value pair and result shape used by hash-table callers.
    struct Entry {
        K first{};
        V second{};

        // Starts with default values when a hash entry is not yet populated.
        Entry() = default;
        // Copies or moves a key/value pair into an entry.
        Entry(const K& key, const V& value) : first(key), second(value) {}
        Entry(K&& key, V&& value)
            : first(move(key)), second(move(value)) {}
    };

    // Gives callers both the stored entry and whether insertion was new.
    struct InsertResult {
        Entry* first;
        bool second;
    };

    using iterator = typename Array<Entry>::iterator;
    using const_iterator = typename Array<Entry>::const_iterator;

    // Iterates over stored entries and reports whether the table has values.
    iterator begin() { return entries_.begin(); }
    const_iterator begin() const { return entries_.begin(); }
    iterator end() { return entries_.end(); }
    const_iterator end() const { return entries_.end(); }
    size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }

    // Looks up a key, returning end() when the key is not present.
    iterator find(const K& key) {
        const size_t index = findIndex(key);
        return index == npos() ? end() : &entries_[index];
    }

    const_iterator find(const K& key) const {
        const size_t index = findIndex(key);
        return index == npos() ? end() : &entries_[index];
    }

    // Returns an existing value or creates a default value for a missing key.
    V& operator[](const K& key) {
        iterator found = find(key);
        if (found != end()) {
            return found->second;
        }
        return emplace(key, V{}).first->second;
    }

    // Reads a key that must exist; a missing key is reported as an error.
    V& at(const K& key) {
        iterator found = find(key);
        if (found == end()) {
            throw out_of_range("Map key was not found.");
        }
        return found->second;
    }

    const V& at(const K& key) const {
        const_iterator found = find(key);
        if (found == end()) {
            throw out_of_range("Map key was not found.");
        }
        return found->second;
    }

    // Adds a key/value pair unless the key is already present.
    template <typename Key, typename Value>
    InsertResult emplace(Key&& key, Value&& value) {
        iterator found = find(key);
        if (found != end()) {
            return {found, false};
        }
        if (buckets_.empty()) {
            rehash(8);
        } else if ((entries_.size() + 1) * 10 >= buckets_.size() * 7) {
            rehash(buckets_.size() * 2);
        }
        const size_t bucket = insertionBucket(key);
        entries_.emplace_back(forward<Key>(key),
                              forward<Value>(value));
        buckets_[bucket] = entries_.size() - 1;
        return {&entries_.back(), true};
    }

    // Drops all entries and buckets so the next insertion starts fresh.
    void clear() {
        entries_.clear();
        buckets_.clear();
    }

private:
    // Uses one sentinel value to represent an unoccupied bucket.
    static constexpr size_t npos() {
        return static_cast<size_t>(-1);
    }

    // Probes forward until it finds the key or reaches an empty bucket.
    size_t findIndex(const K& key) const {
        if (buckets_.empty()) {
            return npos();
        }
        size_t bucket = HashCode<K>{}(key) % buckets_.size();
        const size_t start = bucket;
        while (buckets_[bucket] != npos()) {
            const size_t entryIndex = buckets_[bucket];
            if (entries_[entryIndex].first == key) {
                return entryIndex;
            }
            bucket = (bucket + 1) % buckets_.size();
            if (bucket == start) {
                break;
            }
        }
        return npos();
    }

    // Finds the next open bucket using the same linear-probing rule as lookup.
    size_t insertionBucket(const K& key) const {
        size_t bucket = HashCode<K>{}(key) % buckets_.size();
        while (buckets_[bucket] != npos()) {
            bucket = (bucket + 1) % buckets_.size();
        }
        return bucket;
    }

    // Rebuilds the bucket index after growth without moving the stored entries.
    void rehash(size_t capacity) {
        Array<size_t> nextBuckets;
        nextBuckets.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            nextBuckets.push_back(npos());
        }
        for (size_t i = 0; i < entries_.size(); ++i) {
            size_t bucket =
                HashCode<K>{}(entries_[i].first) % nextBuckets.size();
            while (nextBuckets[bucket] != npos()) {
                bucket = (bucket + 1) % nextBuckets.size();
            }
            nextBuckets[bucket] = i;
        }
        buckets_ = move(nextBuckets);
    }

    Array<Entry> entries_;
    Array<size_t> buckets_;
};

// Stores unique values using a compact array and equality checks.
template <typename T>
class UniqueSet {
public:
    using iterator = typename Array<T>::iterator;
    using const_iterator = typename Array<T>::const_iterator;

    struct InsertResult {
        iterator first;
        bool second;
    };

    UniqueSet() = default;
    // Adds each initial value once, ignoring duplicates in the initializer list.
    UniqueSet(initializer_list<T> values) {
        for (const T& value : values) {
            insert(value);
        }
    }

    // Lets graph traversals visit the set and inspect its current size.
    iterator begin() { return values_.begin(); }
    const_iterator begin() const { return values_.begin(); }
    iterator end() { return values_.end(); }
    const_iterator end() const { return values_.end(); }
    size_t size() const { return values_.size(); }
    bool empty() const { return values_.empty(); }

    // Checks whether a value has already been inserted.
    size_t count(const T& value) const {
        for (const T& current : values_) {
            if (current == value) {
                return 1;
            }
        }
        return 0;
    }

    // Provides a direct membership check for graph traversal and route code.
    bool contains(const T& value) const { return count(value) != 0; }

    // Adds a value only when it is not already in the set.
    InsertResult insert(const T& value) {
        for (iterator current = begin(); current != end(); ++current) {
            if (*current == value) {
                return {current, false};
            }
        }
        values_.push_back(value);
        return {end() - 1, true};
    }

    // Adds a moved value only when an equal value is not already present.
    InsertResult insert(T&& value) {
        for (iterator current = begin(); current != end(); ++current) {
            if (*current == value) {
                return {current, false};
            }
        }
        values_.push_back(move(value));
        return {end() - 1, true};
    }

    // Removes by position or by value, reporting whether a value was found.
    iterator erase(iterator position) { return values_.erase(position); }

    // Finds and removes a matching value, returning zero when it is absent.
    size_t erase(const T& value) {
        for (iterator current = begin(); current != end(); ++current) {
            if (*current == value) {
                values_.erase(current);
                return 1;
            }
        }
        return 0;
    }

    // Clears every value while retaining the array's allocated storage.
    void clear() { values_.clear(); }

private:
    Array<T> values_;
};

// A last-in, first-out collection used by iterative depth-first searches.
template <typename T>
class Stack {
public:
    // Checks whether there are values and reports how many remain.
    bool empty() const { return values_.empty(); }
    size_t size() const { return values_.size(); }
    // Reads or removes the most recently added value.
    T& top() { return values_.back(); }
    const T& top() const { return values_.back(); }
    // Adds a value to the top or removes the current top value.
    void push(const T& value) { values_.push_back(value); }
    // Pushes a value by moving it onto the stack.
    void push(T&& value) { values_.push_back(move(value)); }
    // Removes the current top value; the stack must not be empty.
    void pop() { values_.erase(values_.end() - 1); }

private:
    Array<T> values_;
};

// A binary heap that keeps the element preferred by Compare at its root.
template <typename T, typename Compare>
class BinaryHeap {
public:
    // Checks whether the heap has values and reports its current size.
    bool empty() const { return values_.empty(); }
    size_t size() const { return values_.size(); }
    // Reads the highest-priority value; the heap must not be empty.
    const T& top() const { return values_.front(); }
    // Adds a value or removes the highest-priority value while keeping heap order.
    void push(const T& value) {
        values_.push_back(value);
        siftUp(values_.size() - 1);
    }

    // Pushes a value by moving it into the heap before restoring heap order.
    void push(T&& value) {
        values_.push_back(move(value));
        siftUp(values_.size() - 1);
    }

    // Removes the root and restores the heap order from the new root.
    void pop() {
        if (values_.size() <= 1) {
            values_.clear();
            return;
        }
        values_.front() = move(values_.back());
        values_.erase(values_.end() - 1);
        siftDown(0);
    }

private:
    // Moves a new value upward until its parent has higher priority.
    void siftUp(size_t index) {
        while (index > 0) {
            const size_t parent = (index - 1) / 2;
            if (!compare_(values_[parent], values_[index])) {
                break;
            }
            T value = move(values_[parent]);
            values_[parent] = move(values_[index]);
            values_[index] = move(value);
            index = parent;
        }
    }

    // Moves the root downward until neither child should be promoted.
    void siftDown(size_t index) {
        while (true) {
            const size_t left = index * 2 + 1;
            const size_t right = left + 1;
            size_t best = index;
            if (left < values_.size() &&
                compare_(values_[best], values_[left])) {
                best = left;
            }
            if (right < values_.size() &&
                compare_(values_[best], values_[right])) {
                best = right;
            }
            if (best == index) {
                return;
            }
            T value = move(values_[best]);
            values_[best] = move(values_[index]);
            values_[index] = move(value);
            index = best;
        }
    }

    Array<T> values_;
    Compare compare_{};
};

}  // namespace dsa

#endif
