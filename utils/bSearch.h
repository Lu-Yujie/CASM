class b_search {
public:
    /**
     * Universal binary search returning the index of the first element >= target.
     * Returns 'size' if all elements are smaller than target.
     */
    static inline ui lower_bound_idx(const ui* arr, ui size, ui target) {
        if (size <= 16) { // 阈值通常在 8-32 之间效果较好
            for (ui i = 0; i < size; ++i) {
                if (arr[i] >= target) return i;
            }
            return size;
        } else {
            auto ptr = std::lower_bound(arr, arr + size, target);
            return (ui)(ptr - arr);
        }
    }
};