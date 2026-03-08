#ifndef LU_MEMORY_MANAGER_H
#define LU_MEMORY_MANAGER_H

#include "configuration/types.h"
#include "utils/fastQueue.h"
#include "utils/quick/quickIndex.h"
#include <iostream>
using namespace std;

struct MemoryManager {
    FastCircularQueue<ui> bfs_q;

    vector<bool> flag_array;
    vector<ui> reset_buffer;

    uint64_t* visited_bitmask;
    uint64_t all_visited;
    QuickIndex* quick_index;

    MemoryManager(ui max_cans, ui qnum, ui dnum, const Graph* query_graph) {
        if (qnum > 64) {
            cout << "do not support query with #vertex > 64" << endl;
            exit(-1);
        }
        bfs_q.init(qnum);
        visited_bitmask = new uint64_t[64];
        all_visited = (qnum == 64) ? ~0ULL : ((1ULL << qnum) - 1);
        flag_array.resize(dnum, false);
        reset_buffer.reserve(max_cans);
        quick_index = new QuickIndex(dnum, max_cans, query_graph);
    }
    ~MemoryManager() {
        delete[] visited_bitmask;
        delete quick_index;
    }
};

#endif
