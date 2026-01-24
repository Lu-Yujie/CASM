#ifndef SM_CSMINDEX_H
#define SM_CSMINDEX_H

// 在 filter 生成的 candidate sets 的基础上搞一套索引，反正是离线的，最终的结构应该也是一个 index 结构。
// 默认 index 结构就是每个查询点一组 candidates，然后在 candidates 之间连边的结构。
// filter 规则，往复两轮的一阶邻居判断，不能判断高阶邻居，太慢了，三轮也太慢。
// 第一次先指定一个顺序，从 candidates 最少到最多，然后反向一次
//
#include "graph/graph.h"
using namespace std;

class CSMPruneCache {
public:
    VertexID* order = nullptr;
    const ui** u_cans_nbrs = nullptr;
    ui* u_cans_nbrs_cnt = nullptr;
    bool* results_buffer = nullptr;
    ui* aux_cursors = nullptr;
    ui* aux_queue = nullptr;
    ui qnum = 0;
    ui max_cans = 0;

    CSMPruneCache() {}
    CSMPruneCache(ui max_cans, ui qnum) { update(max_cans, qnum); }
    void update(ui new_max_cans, ui new_qnum);
    ~CSMPruneCache();
};

class CSMIndex {
private:
    ui ** cans = nullptr;
    ui * cans_cnt = nullptr;
    const Graph* query_graph = nullptr;
    ui dnum = 0;
    ui d_edge_num = 0;
    ui max_cans = 0;
    Edges ***edge_matrix = nullptr;
    static CSMPruneCache* pruneCache;
    friend class CSMEngine;
public:
    CSMIndex() {}
    ~CSMIndex() {
        for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
            delete[] cans[i];
        }
        delete[] cans;
        delete[] cans_cnt;
        if (pruneCache != nullptr) {
            delete pruneCache;
            pruneCache = nullptr;
        }
        if (edge_matrix != nullptr) {
            for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
                for (ui j = 0; j < query_graph->getVerticesCount(); ++j) {
                    delete edge_matrix[i][j];
                }
                delete[] edge_matrix[i];
            }
            delete[] edge_matrix;
        }
    }
    bool construct_local(CSMIndex* global);
    void build_A(const Graph *data_graph, const Graph *query_graph);
    void update_A(Update de, vector<Edge>& matched_edges);

private:
    const VertexID* getNeighbors(VertexID u_1, VertexID u_2, VertexID v, ui& nbrs_cnt);
    bool csmPrune(ui u, CSMIndex* global);
    void buildCSMEdgeMatrix(CSMIndex* global);
    inline void copy_block(ui* dst, const ui* src, ui len);
    Edges* insert_blank_row(Edges* old_edges, ui insert_idx);
    Edges* insert_edge_at_index(Edges* old_edges, ui row_idx, ui v_nbr);
    ui ensure_candidate_global(ui u, ui v_can);
    Edges* delete_edge_at_index(Edges* old_edges, ui row_idx, ui v_nbr);
    ui find_candidate_index(ui u, ui v_can);
};

#endif
