#ifndef LU_MEMORY_MANAGER_H
#define LU_MEMORY_MANAGER_H

#include "configuration/types.h"
#include "utils/fastMinHeap.h"
#include "utils/quick/quickIndex.h"
#include <iostream>
using namespace std;

struct MemoryManager {
    FastMinHeap m_heap;

    vector<bool> flag_array;
    vector<ui> reset_buffer;

    uint64_t* visited_bitmask;
    uint64_t all_visited;
    QuickIndex* quick_index;

    vector<vector<VertexID>> cans;
    const Graph *d_graph;
    const Graph *q_graph;
    ui q_num, d_num;

    MemoryManager(ui max_cans, const Graph* query_graph, const Graph *data_graph) {
        q_graph = query_graph;
        d_graph = data_graph;
        q_num = q_graph->getVerticesCount();
        d_num = d_graph->getVerticesCount();

        if (q_num > 64) {
            cout << "do not support query with #vertex > 64" << endl;
            exit(-1);
        }
        cans.resize(q_num);
        for (ui u = 0; u < q_num; u++) {
            cans[u].reserve(max_cans);
        }
        m_heap.init(q_num);
        visited_bitmask = new uint64_t[64];
        all_visited = (q_num == 64) ? ~0ULL : ((1ULL << q_num) - 1);
        flag_array.resize(d_num, false);
        reset_buffer.reserve(max_cans);
        quick_index = new QuickIndex(d_num, max_cans, query_graph);
    }
    ~MemoryManager() {
        delete[] visited_bitmask;
        delete quick_index;
    }
};

#endif
