#ifndef SETOP_H
#define SETOP_H
/**
 * some operations on set
*/
// multi sets union
#include <iostream>
#include <vector>
#include <queue>
#include <climits>
#include <numeric>
#include <algorithm>
#include <string.h>
using namespace std;

typedef uint32_t ui;

class SetOp {
public:

struct ArrayElement {
    ui value;
    ui arrayIndex;
    ui elementIndex;

    ArrayElement(ui v, ui array_idx, ui ele_idx): value(v), arrayIndex(array_idx), elementIndex(ele_idx) {}

    // min-heap
    bool operator>(const ArrayElement& other) const {
        return value > other.value;
    }
};

static
vector<ui> unionMultiple(const vector<vector<ui>*>& arrays) {
    vector<ui> result;
    priority_queue<ArrayElement, vector<ArrayElement>, greater<ArrayElement>> pq;

    // init-pq
    for (ui i = 0; i < arrays.size(); ++i) {
        if (!(*(arrays[i])).empty()) {
            pq.push({(*(arrays[i]))[0], i, 0});
        }
    }

    if (pq.empty()) return result;

    // first element
    ArrayElement current = pq.top();
    pq.pop();
    result.push_back(current.value);
    if (current.elementIndex + 1 < (*(arrays[current.arrayIndex])).size()) {
        pq.push({(*(arrays[current.arrayIndex]))[current.elementIndex + 1], current.arrayIndex, current.elementIndex + 1});
    }

    // scan all array
    while (!pq.empty()) {
        ArrayElement current = pq.top();
        pq.pop();

        if (result.back() != current.value) {
            result.push_back(current.value);
        }

        if (current.elementIndex + 1 < (*(arrays[current.arrayIndex])).size()) {
            pq.push({(*(arrays[current.arrayIndex]))[current.elementIndex + 1], current.arrayIndex, current.elementIndex + 1});
        }
    }

    return result;
}

static
vector<ui> unionMultiple(const ui** arrays, const ui* arrays_size, const ui arrays_num) {
    vector<ui> result;
    priority_queue<ArrayElement, vector<ArrayElement>, greater<ArrayElement>> pq;

    // init-pq
    for (ui i = 0; i < arrays_num; ++i) {
        if (arrays_size[i] != 0) {
            pq.push({arrays[i][0], i, 0});
        }
    }

    if (pq.empty()) return result;

    // first element
    ArrayElement current = pq.top();
    pq.pop();
    result.push_back(current.value);
    if (current.elementIndex + 1 < arrays_size[current.arrayIndex]) {
        pq.push({arrays[current.arrayIndex][current.elementIndex + 1], current.arrayIndex, current.elementIndex + 1});
    }

    // scan all array
    while (!pq.empty()) {
        ArrayElement current = pq.top();
        pq.pop();

        if (result.back() != current.value) {
            result.push_back(current.value);
        }

        // If the current array still has elements, add the next element to the priority queue.
        if (current.elementIndex + 1 < arrays_size[current.arrayIndex]
            // a possible opt, if next value in result.back, push the next one (if rarely happens, it's neg-opt)
            // && result.back() != arrays[current.arrayIndex][current.elementIndex + 1]
            ) {
            pq.push({arrays[current.arrayIndex][current.elementIndex + 1], current.arrayIndex, current.elementIndex + 1});
        }
    }

    // ui* returned = new ui[result.size()];
    // copy(result.begin(), result.end(), returned);
    return result;
}

static
void unionTwoAndUpdate(vector<ui>& array1, vector<ui>& array2) {
    vector<ui> unions;
    ui i = 0, j = 0;
    ui array1_size = array1.size();
    ui array2_size = array2.size();
    while (i < array1_size && j < array2_size) {
        if (array1[i] < array2[j]) {
            unions.push_back(array1[i]);
            ++i;
        } else if (array1[i] > array2[j]) {
            unions.push_back(array2[j]);
            ++j;
        } else {
            unions.push_back(array1[i]);
            ++i;
            ++j;
        }
    }

    // Add remaining elements from array1
    while (i < array1_size) {
        unions.push_back(array1[i]);
        ++i;
    }
    // Add remaining elements from array2
    while (j < array2_size) {
        unions.push_back(array2[j]);
        ++j;
    }
    array1.swap(unions);
}

static
vector<ui> intersectTwo(const ui* array1, const ui* array2, ui array1_size, ui array2_size) {
    vector<ui> intersection;
    ui i = 0, j = 0;
    while (i < array1_size && j < array2_size) {
        if (array1[i] < array2[j]) {
            ++i;
        } else if (array1[i] > array2[j]) {
            ++j;
        } else {
            intersection.push_back(array1[i]);
            ++i;
            ++j;
        }
    }
    return intersection;
}

static
vector<ui> intersectTwo(const vector<ui>& array1, const vector<ui>& array2) {
    vector<ui> intersection;
    ui i = 0, j = 0;
    while (i < array1.size() && j < array2.size()) {
        if (array1[i] < array2[j]) {
            ++i;
        } else if (array1[i] > array2[j]) {
            ++j;
        } else {
            intersection.push_back(array1[i]);
            ++i;
            ++j;
        }
    }
    return intersection;
}

static
vector<ui> intersectTwo(const vector<ui>& array1, const ui* array2, ui array2_size) {
    vector<ui> intersection;
    ui i = 0, j = 0;
    while (i < array1.size() && j < array2_size) {
        if (array1[i] < array2[j]) {
            ++i;
        } else if (array1[i] > array2[j]) {
            ++j;
        } else {
            intersection.push_back(array1[i]);
            ++i;
            ++j;
        }
    }
    return intersection;
}

static
bool haveOverlapTwo(const vector<ui>& array1, const vector<ui>& array2) {
    ui i = 0, j = 0;
    while (i < array1.size() && j < array2.size()) {
        if (array1[i] < array2[j]) {
            ++i;
        } else if (array1[i] > array2[j]) {
            ++j;
        } else {
            ++i;
            ++j;
            return true;
        }
    }
    return false;
}

static
vector<ui> intersectMultiple(const ui** arrays, const ui* arrays_size, const ui arrays_num) {
    vector<ui> result;

    // check arrays.size
    if (arrays_num == 0) return result;
    if (arrays_num == 1) {
        result.reserve(arrays_size[0]);
        for (ui i = 0; i < arrays_size[0]; i++) result.emplace_back(arrays[0][i]);
        return result;
    }

    // init pointer array
    vector<ui> pointers(arrays_num, 0);
    pointers[0] = 1;
    if (arrays_size[0] == 0) return result;
    ui minVal = arrays[0][0];
    ui cnt = 1;
    ui array_idx = 1;

    // until pointers[*] >= arrays[*].size(), aka, one array reach the end
    while (true) {
        // cout << "array_idx: " << array_idx << ", pointers: " << pointers[array_idx] << ", minVal: " << minVal << endl;
        if (pointers[array_idx] >= arrays_size[array_idx]) {
            return result;
        }
        while (arrays[array_idx][pointers[array_idx]] < minVal) {
            pointers[array_idx]++;
            // cout << "jumped: " << arrays[array_idx][pointers[array_idx]] << ", pointers" << pointers[array_idx] << endl;
            if (pointers[array_idx] >= arrays_size[array_idx]) {
                return result;
            }
        }
        if (arrays[array_idx][pointers[array_idx]] != minVal) {
            minVal = arrays[array_idx][pointers[array_idx]];
            cnt = 1;
            // cout << "new minVal: " << minVal << endl;
        } else {
            cnt++;
            if (cnt == arrays_num) {
                result.emplace_back(minVal);
                // next line is right, but whether adding it depends on the expectation of #result
                //   if expectation of #result is small, aka, this check is less than arrays_num, then it worths.
                //   else it does't worth
                // if (pointers[array_idx] == arrays_size[array_idx] - 1) return result;
            }
        }
        pointers[array_idx]++;
        array_idx++;
        array_idx%=arrays_num;
    }
    return result;
}

static // A = A-B
void setDifference(vector<ui>& A,vector<ui>& B) {
    ui i = 0;  // idx_A
    ui j = 0;  // idx_B
    ui k = 0;  // idx_result, also size
    auto A_size = A.size();
    auto B_size = B.size();

    while (i < A_size && j < B_size) {
        if (A[i] < B[j]) {  // A[i] not in B, add to result(k)
            A[k++] = A[i++];
        }
        else if (A[i] > B[j]) {  // A[i] > B[j], next ele of B
            j++;
        }
        else {  // A[i] == B[j], delete A[i]
            i++;
        }
    }

    // add remained eles of A
    while (i < A_size) {
        A[k++] = A[i++];
    }
    A.resize(k);
}

static
void setDifference(ui* A, ui& A_size, vector<ui>&B) {
    ui i = 0;  // idx_A
    ui j = 0;  // idx_B
    ui k = 0;  // idx_result, also size
    ui B_size = B.size();

    while (i < A_size && j < B_size) {
        if (A[i] < B[j]) {  // A[i] not in B, add to result(k)
            A[k++] = A[i++];
        }
        else if (A[i] > B[j]) {  // A[i] > B[j], next ele of B
            j++;
        }
        else {  // A[i] == B[j], delete A[i]
            i++;
        }
    }

    // add remained eles of A
    while (i < A_size) {
        A[k++] = A[i++];
    }
    A_size = k;
}

static // A = A∩B
void intersectAndUpdate(vector<ui>&A, const vector<ui>&B) {
    ui indexA = 0;
    ui indexB = 0;
    ui insertPos = 0;
    ui A_size = A.size();
    ui B_size = B.size();

    while (indexA < A_size && indexB < B_size) {
        if (A[indexA] < B[indexB]) {
            indexA++;
        }
        else if (A[indexA] > B[indexB]) {
            indexB++;
        }
        else {
            A[insertPos++] = A[indexA];
            indexA++;
            indexB++;
        }
    }

    A.resize(insertPos);
    return;
}

static
bool setInclude(const ui* little, ui l_size, const ui* big, ui b_size) {
    if (b_size < l_size) return false;
    for (ui i = 0; i < l_size; i++) {
        bool found = false;
        for (ui j = 0; j < b_size; j++) {
            if (big[j] == little[i]) {
                found = true;
                break;
            }
        }
        if(!found) return false;
    }
    return true;
}

// judge each array in arrays have overlap with arrya2 or not
// should only be used when array2 is long
static
vector<bool> multiOverlap(const vector<vector<ui>*> arrays, const vector<ui>& array2) {
    ui num = arrays.size();
    vector<bool> overlap(num, false); // 初始化为 false
    vector<size_t> idxs(num, 0);      // 记录每个 array 目前遍历到的位置

    ui overlap_num = 0;       // 已经找到交集的数组数量
    ui unoverlapped_num = 0;  // 已经遍历完且无交集的数组数量

    // all_arrays 存储 3 部分: [unoverlapped, remained, overlapped]
    vector<size_t> all_arrays(num);
    iota(all_arrays.begin(), all_arrays.end(), 0); // 填入 0, 1, 2... num-1

    ui a2idx = 0;
    while (overlap_num + unoverlapped_num < num && a2idx < array2.size()) {
        ui cur_value = array2[a2idx];

        // 我们只遍历 all_arrays 中间 "remained" 的部分
        size_t i = unoverlapped_num;
        size_t end_boundary = num - overlap_num;
        while (i < end_boundary) {
            ui arr_idx = all_arrays[i]; // 获取原始数组的 ID
            const vector<ui>& curr_arr = *arrays[arr_idx]; // 获取当前数组引用
            size_t& ptr = idxs[arr_idx]; // 获取当前数组的指针引用

            // 1. 让当前数组的指针追赶 cur_value
            while (ptr < curr_arr.size() && curr_arr[ptr] < cur_value) {
                ptr++;
            }

            // 2. 检查状态
            if (ptr == curr_arr.size()) {
                // 情况 A: 数组已耗尽，确定无交集
                // 把它交换到左侧 (unoverlapped 区域)
                swap(all_arrays[i], all_arrays[unoverlapped_num]);
                unoverlapped_num++;
                i++;
            } else if (curr_arr[ptr] == cur_value) {
                // 情况 B: 发现交集
                overlap[arr_idx] = true;
                
                // 把它交换到右侧 (overlapped 区域), 所以 i 不递增
                end_boundary--; // 右边界左移
                swap(all_arrays[i], all_arrays[end_boundary]);
                overlap_num++;
            } else {
                // 情况 C: curr_arr[ptr] > cur_value
                i++;
            }
        }
        a2idx++;
    }
    return overlap;
}

/**
 * 高性能版：调用者提供 buffer，函数内部无内存分配。
 * @param results_buffer  [输出] 结果数组，长度需 >= num_candidates
 * @param aux_cursors     [辅助] 游标数组，长度需 >= num_candidates
 * @param aux_queue       [辅助] 队列数组，长度需 >= num_candidates
 * @return found_count, 返回有多少数组存在交集
 */
static
ui multi_overlap_no_alloc(const ui** candidates, ui* candidate_sizes, ui num_candidates, 
                          ui* target, ui target_size,
                          bool* results_buffer, 
                          ui* aux_cursors, 
                          ui* aux_queue) {
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
                swap(aux_queue[i], aux_queue[exhausted_count]);
                exhausted_count++;
                i++;
            } else if (curr_arr[ptr] == target_val) {
                results_buffer[id] = true;
                active_end--;
                swap(aux_queue[i], aux_queue[active_end]);
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
 * 高性能版：适配 std::vector 结构的 multi_overlap
 * @param candidates      [输入] 候选点的邻居列表集合 (u_cans_nbrs)
 * @param num_candidates  [输入] 当前有效的候选点数量 (valid_cans_cnt)
 * @param target          [输入] 目标集合 (unbr_candidates)
 * @param results_buffer  [输出] 结果 buffer (vector<bool>)
 * @param aux_cursors     [辅助] 游标 buffer
 * @param aux_queue       [辅助] 队列 buffer
 * @return found_count    存在交集的数量
 */
static
ui multi_overlap_no_alloc(const vector<const vector<VertexID>*>& candidates, 
                          ui num_candidates, 
                          const vector<VertexID>& target,
                          vector<bool>& results_buffer, 
                          vector<ui>& aux_cursors, 
                          vector<ui>& aux_queue) {
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
                swap(aux_queue[i], aux_queue[exhausted_count]);
                exhausted_count++;
                i++;
            } else if (curr_arr[ptr] == target_val) {
                results_buffer[id] = true;
                active_end--;
                swap(aux_queue[i], aux_queue[active_end]);
                found_count++;
            } else {
                i++;
            }
        }
        target_idx++;
    }
    return found_count;
}

}; // class SetOp

#endif
