#ifndef CSM_ENGINE_H
#define CSM_ENGINE_H
#include "CSMIndex.h"
#include "gmp.h"

// auxiliary structure for CSM
// 更新的数据边可以匹配到多个查询边上，
class CSMEngine {
private:
    CSMIndex* global_index;
    CSMIndex* local_index;
    ui*local_max_cans;
    vector<vector<Edge>> automorphism_edges_;
    unordered_map<ELabel, vector<Edge>> label_edge_mapping_;
    unordered_map<ELabel, vector<uint32_t>> label_automorphism_mapping_;
public:
    CSMEngine(): global_index(nullptr), local_index(nullptr), label_edge_mapping_({}), label_automorphism_mapping_({}) {}
    ~CSMEngine() {
        delete global_index;
        delete local_index;
        delete[] local_max_cans;
    }
    void init(const Graph *data_graph, const Graph *query_graph);
    void query(Update de, size_t output_limit, mpz_t embedding_cnt, int64_t& end_time);

private:
    void build_automorphism_edges(const Graph *query_graph);
    void build_edges_mapping(const Graph *query_graph);
    bool try_build_local(Edge de, Edge qe);
};

#endif