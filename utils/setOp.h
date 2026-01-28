#ifndef SETOP_H
#define SETOP_H

#include <vector>
#include <cstdint>

using namespace std;

typedef unsigned int ui;
typedef uint32_t VertexID;

class SetOp {
public:
    // 排序与去重
    static void sortAndUnique(vector<ui>& vec);

    // 多集合并集 (Union)
    static vector<ui> unionMultiple(const vector<vector<ui>*>& arrays);
    static vector<ui> unionMultiple(const ui** arrays, const ui* arrays_size, ui arrays_num);

    // A = A U B
    static void unionTwoAndUpdate(vector<ui>& array1, vector<ui>& array2);

    // 交集 (Intersection)
    static vector<ui> intersectTwo(const ui* array1, const ui* array2, ui array1_size, ui array2_size);
    static vector<ui> intersectTwo(const vector<ui>& array1, const vector<ui>& array2);
    static vector<ui> intersectTwo(const vector<ui>& array1, const ui* array2, ui array2_size);

    // A = A n B
    static void intersectAndUpdate(vector<ui>& A, const vector<ui>& B);

    // 判断是否有交集
    static bool haveOverlapTwo(const vector<ui>& array1, const vector<ui>& array2);

    // 子集判断 (判断 little 是否被 big 包含)
    static bool setInclude(const ui* little, ui l_size, const ui* big, ui b_size);

    // A = A - B
    static void setDifference(vector<ui>& A, vector<ui>& B);
    static void setDifference(ui* A, ui& A_size, vector<ui>& B);

    // 多集操作 (Advanced Overlap)
    static vector<ui> intersectMultiple(const ui** arrays, const ui* arrays_size, ui arrays_num);

    // 批量重叠检测
    static vector<bool> multiOverlap(const vector<vector<ui>*> arrays, const vector<ui>& array2);

    // 高性能无分配重叠检测 (Raw Ptr 版本)
    static ui multi_overlap_no_alloc(const ui** candidates, ui* candidate_sizes, ui num_candidates,
                                     ui* target, ui target_size,
                                     bool* results_buffer,
                                     ui* aux_cursors,
                                     ui* aux_queue);

    // 高性能无分配重叠检测 (Vector 版本)
    static ui multi_overlap_no_alloc(const vector<const vector<ui>*>& candidates,
                                     ui num_candidates,
                                     const vector<ui>& target,
                                     vector<bool>& results_buffer,
                                     vector<ui>& aux_cursors,
                                     vector<ui>& aux_queue);
};

#endif // SETOP_H