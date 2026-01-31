#include <vector>
#include <cassert>
// 无安全检查的快速队列，固定最大长度
template <typename T>
class FastCircularQueue {
private:
    std::vector<T> _data;
    int _head;
    int _tail;
    int _count;
    int _capacity;

public:
    FastCircularQueue() : _head(0), _tail(0), _count(0), _capacity(0) {}
    void init(int max_size) {
        if (max_size > _capacity) {
            _data.resize(max_size);
            _capacity = max_size;
        }
        clear();
    }

    inline void clear() {
        _head = 0;
        _tail = 0;
        _count = 0;
    }

    inline bool empty() const {
        return _count == 0;
    }

    inline int size() const {
        return _count;
    }

    inline void push(T val) {
        // assert(_count < _capacity && "Queue overflow: logic error or buffer too small");
        _data[_tail] = val;
        _tail++;
        if (_tail == _capacity) _tail = 0; // 避免使用 % 操作符，分支预测更快
        _count++;
    }

    inline void pop() {
        // assert(_count > 0);
        _head++;
        if (_head == _capacity) _head = 0;
        _count--;
    }

    inline T front() const {
        return _data[_head];
    }
};