#include "setOp.h"
#include <queue>
#include <algorithm>
#include <cstring>
#include <numeric>
using namespace std;

// =========================================================
// 内部通用模板 (仅用于普通的集合操作，减少非核心路径的重复)
// =========================================================
namespace {

    // 通用交集逻辑
    template <typename InputIt1, typename InputIt2, typename OutputContainer>
    void impl_intersect(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, OutputContainer& out) {
        while (first1 != last1 && first2 != last2) {
            if (*first1 < *first2) {
                ++first1;
            } else if (*first1 > *first2) {
                ++first2;
            } else {
                out.push_back(*first1);
                ++first1;
                ++first2;
            }
        }
    }
    template <typename InputIt1, typename InputIt2>
    ui impl_intersect(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, ui* buffer) {
        ui cnt = 0;
        while (first1 != last1 && first2 != last2) {
            if (*first1 < *first2) {
                ++first1;
            } else if (*first1 > *first2) {
                ++first2;
            } else {
                buffer[cnt++] = *first1; // 直接写入 buffer 并递增计数
                ++first1;
                ++first2;
            }
        }
        return cnt;
    }

    // 通用差集逻辑: A = A - B
    template <typename ContainerA, typename InputItB>
    void impl_difference_vector(ContainerA& A, InputItB firstB, InputItB lastB) {
        auto itA = A.begin();
        auto endA = A.end();
        auto itB = firstB;
        auto out = A.begin(); 

        while (itA != endA && itB != lastB) {
            if (*itA < *itB) {
                *out++ = *itA++; 
            } else if (*itA > *itB) {
                ++itB;           
            } else {
                ++itA;           
                ++itB;
            }
        }
        while (itA != endA) {
            *out++ = *itA++;
        }
        A.resize(std::distance(A.begin(), out));
    }
    
    // 差集逻辑 (Raw Pointer 版本)
    template <typename InputItB>
    void impl_difference_ptr(ui* A, ui& A_size, InputItB firstB, InputItB lastB) {
        ui i = 0, k = 0; 
        while (i < A_size && firstB != lastB) {
            ui b_val = *firstB;
            if (A[i] < b_val) {
                A[k++] = A[i++];
            } else if (A[i] > b_val) {
                ++firstB;
            } else {
                i++;
                ++firstB;
            }
        }
        while (i < A_size) {
            A[k++] = A[i++];
        }
        A_size = k;
    }

    // 多路归并使用的堆元素
    struct HeapElement {
        ui value;
        ui arrayIndex;     
        ui nextElemIndex;  

        bool operator>(const HeapElement& other) const {
            return value > other.value;
        }
    };

    // 通用多路合并逻辑
    template <typename SizeAccessor, typename ValAccessor>
    vector<ui> impl_union_multiple(ui num_arrays, SizeAccessor getSize, ValAccessor getValue) {
        vector<ui> result;
        priority_queue<HeapElement, vector<HeapElement>, greater<HeapElement>> pq;

        for (ui i = 0; i < num_arrays; ++i) {
            if (getSize(i) > 0) {
                pq.push({getValue(i, 0), i, 0});
            }
        }

        if (pq.empty()) return result;

        HeapElement current = pq.top();
        pq.pop();
        result.push_back(current.value);

        if (current.nextElemIndex + 1 < getSize(current.arrayIndex)) {
            pq.push({getValue(current.arrayIndex, current.nextElemIndex + 1), 
                     current.arrayIndex, current.nextElemIndex + 1});
        }

        while (!pq.empty()) {
            current = pq.top();
            pq.pop();

            if (result.back() != current.value) {
                result.push_back(current.value);
            }

            if (current.nextElemIndex + 1 < getSize(current.arrayIndex)) {
                pq.push({getValue(current.arrayIndex, current.nextElemIndex + 1), 
                         current.arrayIndex, current.nextElemIndex + 1});
            }
        }
        return result;
    }
} // end anonymous namespace

// =========================================================
// SetOp 类成员实现
// =========================================================

void SetOp::sortAndUnique(vector<ui>& vec) {
    if (vec.empty()) return;
    std::sort(vec.begin(), vec.end());
    auto last = std::unique(vec.begin(), vec.end());
    vec.erase(last, vec.end());
}

// ---------------- Union Multiple ----------------

vector<ui> SetOp::unionMultiple(const vector<vector<ui>*>& arrays) {
    auto getSize = [&](ui i) { return arrays[i]->size(); };
    auto getValue = [&](ui i, ui idx) { return (*arrays[i])[idx]; };
    return impl_union_multiple(arrays.size(), getSize, getValue);
}

vector<ui> SetOp::unionMultiple(const ui** arrays, const ui* arrays_size, const ui arrays_num) {
    auto getSize = [&](ui i) { return arrays_size[i]; };
    auto getValue = [&](ui i, ui idx) { return arrays[i][idx]; };
    return impl_union_multiple(arrays_num, getSize, getValue);
}

// ---------------- Union Two ----------------

void SetOp::unionTwoAndUpdate(vector<ui>& array1, vector<ui>& array2) {
    vector<ui> unions;
    unions.reserve(array1.size() + array2.size()); 
    
    auto i1 = array1.begin(), end1 = array1.end();
    auto i2 = array2.begin(), end2 = array2.end();
    
    while (i1 != end1 && i2 != end2) {
        if (*i1 < *i2) {
            unions.push_back(*i1++);
        } else if (*i1 > *i2) {
            unions.push_back(*i2++);
        } else {
            unions.push_back(*i1++);
            i2++;
        }
    }
    unions.insert(unions.end(), i1, end1);
    unions.insert(unions.end(), i2, end2);
    
    array1.swap(unions);
}

// ---------------- Intersect Two ----------------

vector<ui> SetOp::intersectTwo(const ui* array1, const ui* array2, ui array1_size, ui array2_size) {
    vector<ui> result;
    impl_intersect(array1, array1 + array1_size, array2, array2 + array2_size, result);
    return result;
}

vector<ui> SetOp::intersectTwo(const vector<ui>& array1, const vector<ui>& array2) {
    vector<ui> result;
    impl_intersect(array1.begin(), array1.end(), array2.begin(), array2.end(), result);
    return result;
}

vector<ui> SetOp::intersectTwo(const vector<ui>& array1, const ui* array2, ui array2_size) {
    vector<ui> result;
    impl_intersect(array1.begin(), array1.end(), array2, array2 + array2_size, result);
    return result;
}

ui SetOp::intersectTwo(const ui* array1, const ui* array2, ui array1_size, ui array2_size, ui* buffer) {
    return impl_intersect(array1, array1 + array1_size, array2, array2 + array2_size, buffer);
}

ui SetOp::intersectTwo(const vector<ui>& array1, const vector<ui>& array2, ui* buffer) {
    return impl_intersect(array1.begin(), array1.end(), array2.begin(), array2.end(), buffer);
}

ui SetOp::intersectTwo(const vector<ui>& array1, const ui* array2, ui array2_size, ui* buffer) {
    return impl_intersect(array1.begin(), array1.end(), array2, array2 + array2_size, buffer);
}

void SetOp::intersectAndUpdate(vector<ui>& A, const vector<ui>& B) {
    ui insertPos = 0;
    auto itA = A.begin();
    auto itB = B.begin();
    auto endA = A.end();
    auto endB = B.end();

    while (itA != endA && itB != endB) {
        if (*itA < *itB) {
            ++itA;
        } else if (*itA > *itB) {
            ++itB;
        } else {
            A[insertPos++] = *itA;
            ++itA;
            ++itB;
        }
    }
    A.resize(insertPos);
}

// ---------------- Misc ----------------

bool SetOp::haveOverlapTwo(const vector<ui>& array1, const vector<ui>& array2) {
    auto it1 = array1.begin(), end1 = array1.end();
    auto it2 = array2.begin(), end2 = array2.end();
    while (it1 != end1 && it2 != end2) {
        if (*it1 < *it2) ++it1;
        else if (*it1 > *it2) ++it2;
        else return true;
    }
    return false;
}

bool SetOp::setInclude(const ui* little, ui l_size, const ui* big, ui b_size) {
    if (b_size < l_size) return false;
    ui i = 0, j = 0;
    while (i < l_size && j < b_size) {
        if (little[i] < big[j]) return false;
        if (little[i] > big[j]) j++;
        else { i++; j++; }
    }
    return i == l_size;
}

// ---------------- Set Difference ----------------

void SetOp::setDifference(vector<ui>& A, vector<ui>& B) {
    impl_difference_vector(A, B.begin(), B.end());
}

void SetOp::setDifference(ui* A, ui& A_size, vector<ui>& B) {
    impl_difference_ptr(A, A_size, B.begin(), B.end());
}

// ---------------- Intersect Multiple ----------------

vector<ui> SetOp::intersectMultiple(const ui** arrays, const ui* arrays_size, const ui arrays_num) {
    vector<ui> result;
    if (arrays_num == 0) return result;
    if (arrays_num == 1) {
        result.assign(arrays[0], arrays[0] + arrays_size[0]);
        return result;
    }

    vector<ui> pointers(arrays_num, 0);
    if (arrays_size[0] == 0) return result;
    
    pointers[0] = 0;
    ui minVal = arrays[0][0];
    ui matchCount = 1;
    ui array_idx = 1;

    while (true) {
        ui& ptr = pointers[array_idx];
        const ui size = arrays_size[array_idx];
        const ui* arr = arrays[array_idx];

        if (ptr >= size) return result;

        while (ptr < size && arr[ptr] < minVal) {
            ptr++;
        }
        if (ptr >= size) return result;

        if (arr[ptr] > minVal) {
            minVal = arr[ptr];
            matchCount = 1;
        } else {
            matchCount++;
            if (matchCount == arrays_num) {
                result.push_back(minVal);
            }
        }
        
        array_idx = (array_idx + 1) % arrays_num;
        
        if (matchCount == arrays_num) {
             pointers[array_idx]++; 
             matchCount = 0;
             if (pointers[array_idx] >= arrays_size[array_idx]) return result;
             minVal = arrays[array_idx][pointers[array_idx]];
             matchCount = 1;
             array_idx = (array_idx + 1) % arrays_num;
        }
    }
}

// ---------------- Multi Overlap ----------------
vector<bool> SetOp::multiOverlap(const vector<vector<ui>*> arrays, const vector<ui>& array2) {
    ui num = arrays.size();
    vector<bool> overlap(num, false);
    vector<size_t> idxs(num, 0);

    ui overlap_num = 0;
    ui unoverlapped_num = 0;

    vector<size_t> all_arrays(num);
    iota(all_arrays.begin(), all_arrays.end(), 0);  // fill 0,1,2,...

    ui a2idx = 0;
    while (overlap_num + unoverlapped_num < num && a2idx < array2.size()) {
        ui cur_value = array2[a2idx];

        size_t i = unoverlapped_num;
        size_t end_boundary = num - overlap_num;
        while (i < end_boundary) {
            ui arr_idx = all_arrays[i];
            const vector<ui>& curr_arr = *arrays[arr_idx];
            size_t& ptr = idxs[arr_idx];

            while (ptr < curr_arr.size() && curr_arr[ptr] < cur_value) {
                ptr++;
            }

            if (ptr == curr_arr.size()) {
                swap(all_arrays[i], all_arrays[unoverlapped_num]);
                unoverlapped_num++;
                i++;
            } else if (curr_arr[ptr] == cur_value) {
                overlap[arr_idx] = true;
                end_boundary--;
                swap(all_arrays[i], all_arrays[end_boundary]);
                overlap_num++;
            } else {
                i++;
            }
        }
        a2idx++;
    }
    return overlap;
}

// High Performance Multi Overlap (No Alloc)
/**
 * raw pointer
 */
ui SetOp::multi_overlap_no_alloc(const ui** candidates, ui* candidate_sizes, ui num_candidates, 
                                 ui* target, ui target_size,
                                 bool* results_buffer, 
                                 ui* aux_cursors, 
                                 ui* aux_queue) {
    // 内存初始化
    memset(results_buffer, 0, num_candidates * sizeof(bool));
    memset(aux_cursors, 0, num_candidates * sizeof(ui));

    for (ui k = 0; k < num_candidates; ++k) {
        aux_queue[k] = k;
    }

    ui found_count = 0;
    ui exhausted_count = 0;
    ui target_idx = 0;

    while ((found_count + exhausted_count < num_candidates) && (target_idx < target_size)) {
        ui target_val = target[target_idx];

        size_t i = exhausted_count;
        size_t active_end = num_candidates - found_count;

        while (i < active_end) {
            ui id = aux_queue[i];
            const ui* curr_arr = candidates[id];
            ui size = candidate_sizes[id];
            ui& ptr = aux_cursors[id];

            while (ptr < size && curr_arr[ptr] < target_val) {
                ptr++;
            }

            if (ptr == size) {
                std::swap(aux_queue[i], aux_queue[exhausted_count]);
                exhausted_count++;
                i++;
            } else if (curr_arr[ptr] == target_val) {
                results_buffer[id] = true;
                active_end--;
                std::swap(aux_queue[i], aux_queue[active_end]);
                found_count++;
            } else {
                i++;
            }
        }
        target_idx++;
    }
    return found_count;
}

/**
 * Vector
 */
ui SetOp::multi_overlap_no_alloc(const vector<const vector<ui>*>& candidates, 
                                 ui num_candidates, 
                                 const vector<ui>& target,
                                 vector<bool>& results_buffer, 
                                 vector<ui>& aux_cursors, 
                                 vector<ui>& aux_queue) {
    // 内存初始化 (注意 vector<bool> 不可用 memset)
    std::fill(results_buffer.begin(), results_buffer.begin() + num_candidates, false);
    std::fill(aux_cursors.begin(), aux_cursors.begin() + num_candidates, 0);
    
    for (ui k = 0; k < num_candidates; ++k) {
        aux_queue[k] = k;
    }

    ui found_count = 0;
    ui exhausted_count = 0;
    ui target_idx = 0;

    const VertexID* target_data = target.data();
    ui target_size = target.size();

    while ((found_count + exhausted_count < num_candidates) && (target_idx < target_size)) {
        ui target_val = target_data[target_idx];
        
        size_t i = exhausted_count;
        size_t active_end = num_candidates - found_count;

        while (i < active_end) {
            ui id = aux_queue[i];

            const vector<VertexID>* vec_ptr = candidates[id];
            const VertexID* curr_arr = vec_ptr->data();
            ui size = vec_ptr->size(); 

            ui& ptr = aux_cursors[id];

            while (ptr < size && curr_arr[ptr] < target_val) {
                ptr++;
            }

            if (ptr == size) {
                std::swap(aux_queue[i], aux_queue[exhausted_count]);
                exhausted_count++;
                i++;
            } else if (curr_arr[ptr] == target_val) {
                results_buffer[id] = true;
                active_end--;
                std::swap(aux_queue[i], aux_queue[active_end]);
                found_count++;
            } else {
                i++;
            }
        }
        target_idx++;
    }
    return found_count;
}