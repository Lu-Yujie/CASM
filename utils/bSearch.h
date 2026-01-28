#include <vector>
#include <algorithm>
using ui = unsigned int; 

class b_search {
public:
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

    /**
     * Vector 包装版本
     */
    template<class T>
    static inline ui lower_bound_idx(const std::vector<T>& arr, T target) {
        return lower_bound_idx(arr.data(), (ui)arr.size(), target);
    }
};