#ifndef QUICK_VALIDCANS_H
#define QUICK_VALIDCANS_H

#include <cstring>
#include <algorithm>
#include <cassert>
#include "configuration/types.h"

struct ValidCans {
public:
    // 核心数据成员
    VertexID** cans;     // 这里的 cans[i] 只是指向 _pool 中特定位置的指针，本身不持有数据
    ui* cans_cnt;        // 记录每一层候选集的大小

private:
    VertexID* _pool;     // 【核心优化】由于最大深度和最大宽度已知，我们只申请这唯一一块连续的大内存
    ui _capacity;        // max_cans (每一层的最大容量)
    ui _max_depth;       // qnum (最大递归深度/查询点数量)
    ui _cursor;         // 当前栈顶指针 (对应当前递归层数)

public:
    // 构造函数：一次性分配所有内存
    ValidCans(ui max_cans, ui qnum) : _capacity(max_cans), _max_depth(qnum), _cursor(0), _pool(nullptr) {
        cans_cnt = new ui[_max_depth];
        cans = new VertexID*[_max_depth];

        cans[0] = nullptr;
        cans_cnt[0] = 0;

        if (_max_depth > 1) {
            size_t pool_size = (size_t)(_max_depth - 1) * _capacity;
            _pool = new VertexID[pool_size];

            // 将指针挂载到内存池上
            for (ui i = 1; i < _max_depth; i++) {
                cans[i] = _pool + (size_t)(i - 1) * _capacity;
                cans_cnt[i] = 0;
            }
        }
    }

    ~ValidCans() {
        if (_pool) delete[] _pool;     // 只需要 delete 这一块大内存
        delete[] cans;      // delete 指针数组
        delete[] cans_cnt;  // delete 计数数组
    }

    // 导入根节点候选集 (Zero Copy)
    inline void importRootCandidates(VertexID* external_ptr, ui count) {
        cans[0] = external_ptr; 
        cans_cnt[0] = count;
        _cursor = 0;
    }

    // ==========================================
    // Stack 操作接口 (用于递归/循环)
    // ==========================================

    // 获得当前层只读指针 (用于遍历当前候选)
    inline const VertexID* cur_cans() const {
        return cans[_cursor];
    }

    inline ui cur_cans_cnt() const {
        return cans_cnt[_cursor];
    }

    // 获得下一层写入指针 (用于生成下一层候选)
    // 注意：调用此函数时，_cursor 还没变
    inline VertexID* next_buffer() {
        assert(_cursor + 1 < _max_depth);
        return cans[_cursor + 1];
    }

    inline ui& next_buffer_cnt() {
        assert(_cursor + 1 < _max_depth);
        return cans_cnt[_cursor + 1];
    }

    // 确认推入下一层 (Push)
    // 参数：next_layer_count: 在 next_buffer() 里填入的元素个数
    inline void push() {
        _cursor++;
    }

    // 回溯 (Pop)
    inline void pop() {
        assert(_cursor > 0);
        _cursor--;
    }

    // 获取当前深度
    inline int depth() const {
        return _cursor;
    }
};

#endif
