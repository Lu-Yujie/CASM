
#ifndef CSM_FAST_MIN_HEAP
#define CSM_FAST_MIN_HEAP

#include <vector>
#include <cassert>

class FastMinHeap {
    typedef unsigned int ui;
private:
    struct Node {
        ui v;
        ui score;
    };
    std::vector<Node> _data;
    int _count;
    int _capacity;

public:
    FastMinHeap() : _count(0), _capacity(0) {}

    void init(int max_size) {
        if (max_size > _capacity) {
            _data.resize(max_size);
            _capacity = max_size;
        }
        clear();
    }

    inline void clear() {
        _count = 0;
    }

    inline bool empty() const {
        return _count == 0;
    }

    inline int size() const {
        return _count;
    }

    // 入堆：O(log N)
    inline void push(ui v, ui score) {
        int i = _count++;
        while (i > 0) {
            int p = (i - 1) >> 1;
            if (_data[p].score <= score) break;
            _data[i] = _data[p];
            i = p;
        }
        _data[i] = {v, score};
    }

    // 出堆：O(log N)
    inline ui pop() {
        ui ret = _data[0].v;
        _count--;
        if (_count > 0) {
            Node last = _data[_count];
            int i = 0;
            while ((i << 1) + 1 < _count) {
                int left = (i << 1) + 1;
                int right = left + 1;
                int min_child = (right < _count && _data[right].score < _data[left].score) ? right : left;
                if (_data[min_child].score >= last.score) break;
                _data[i] = _data[min_child];
                i = min_child;
            }
            _data[i] = last;
        }
        return ret;
    }
};

#endif