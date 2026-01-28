class b_search {
public:
    /**
     * Universal binary search returning the index of the first element >= target.
     * Returns 'size' if all elements are smaller than target.
     */
    template<class T>
    static inline ui lower_bound_idx(const T* arr, ui size, T target) {
        if (size <= 16) {
            for (ui i = 0; i < size; ++i) {
                if (arr[i] >= target) return i;
            }
            return size;
        } else {
            auto ptr = std::lower_bound(arr, arr + size, target);
            return (ui)(ptr - arr);
        }
    }

    template<class T>
    static inline ui lower_bound_idx(vector<T> arr, T target) {
        auto size = arr.size();
        if (size <= 16) {
            for (ui i = 0; i < size; ++i) {
                if (arr[i] >= target) return i;
            }
            return size;
        } else {
            auto ptr = std::lower_bound(arr.begin(), arr.end(), target);
            return (ui)(ptr - arr.begin());
        }
    }
};