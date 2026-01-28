#ifndef SM_CSMINDEX_H
#define SM_CSMINDEX_H

// 在 filter 生成的 candidate sets 的基础上搞一套索引，反正是离线的，最终的结构应该也是一个 index 结构。
// 默认 index 结构就是每个查询点一组 candidates，然后在 candidates 之间连边的结构。
// filter 规则，往复两轮的一阶邻居判断，不能判断高阶邻居，太慢了，三轮也太慢。
// 第一次先指定一个顺序，从 candidates 最少到最多，然后反向一次
//
#include "graph/graph.h"
#include <queue>
using namespace std;

struct Aux {
    CSMEdges*** data = nullptr;
    vector<vector<VertexID>> cans;
    vector<VertexID> empty;
    const Graph* query_graph = nullptr;
    ui dnum, qnum;
    Aux() {}
    ~Aux() {
        if (data != nullptr) {
            for (ui i = 0; i < qnum; i++) {
                for (ui j = 0; j < qnum; j++) { if (data[i][j]) delete data[i][j];}
                delete[] data[i];
            }
            delete[] data;
        }
    }
    void init(const Graph* data_graph, const Graph* query_graph);
    void init(const Graph* query_graph);
    void update(const Aux& global);
    const vector<VertexID>& getNeighbors(VertexID u_1, VertexID u_2, VertexID v) const;
};

struct CSMPruneCache {
    VertexID* order = nullptr;
    vector<bool> visited;
    queue<ui> bfs_q;
    vector<ui> flag_array;
    vector<ui> reset_buffer;
    vector<VertexID> valid_cans_buffer;

    CSMPruneCache() {}
    CSMPruneCache(ui max_cans, ui qnum, ui dnum);
    ~CSMPruneCache() { delete[] order; }
};

class CSMIndex {
private:
    Aux aux;
    static CSMPruneCache* pruneCache;
    friend class CSMEngine;
public:
    CSMIndex() {}
    ~CSMIndex() {
        if (pruneCache != nullptr) {
            delete pruneCache;
            pruneCache = nullptr;
        }
    }
    bool try_build_local(const CSMIndex* global, Edge de, Edge qe);
    void build_Aux(const Graph *data_graph, const Graph *query_graph);
    void update_Aux(Update de, vector<Edge>& matched_edges);

private:
    bool propagate_forward(const CSMIndex* global, ui u, ui unbr);
    bool propagate_neighbor_constraint(const CSMIndex* global, ui u_fixed, VertexID v_fixed);
    bool edge_quick_prune(const CSMIndex* global, Edge de, Edge qe);
    ui ensure_candidate_global(ui u, ui v_can);
    void insert_edge_at_index(CSMEdges* old_edges, ui row_idx, ui v_nbr);
    void delete_edge_at_index(CSMEdges* old_edges, ui row_idx, ui v_nbr);
    ui find_candidate_index(ui u, ui v_can);
};

#endif
