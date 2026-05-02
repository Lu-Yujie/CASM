#ifndef LU_AUX_H
#define LU_AUX_H

#include "graph/graph.h"
#include <absl/container/flat_hash_set.h>

struct Aux {
    std::vector<std::vector<CSMEdges*>> label_edges;  // [src_label][dst_label]
    std::vector<LabelID> q_labels;
    vector<VertexID> empty;
    ui vlabels_count = 0;

    Aux() {}
    ~Aux();
    void buildData(const Graph* query_graph, const Graph* data_graph, uint64_t* visited);
    const vector<VertexID>& getNeighbors(VertexID u, VertexID u_nbr, VertexID v) const {
        auto edges = label_edges[q_labels[u]][q_labels[u_nbr]];
        if (edges != nullptr) {
            auto it = edges->edge_map.find(v);
            if (it != edges->edge_map.end()) {
                return it->second;
            }
        }
        return empty;
    }
    inline void insert_edge(LabelID src_label, LabelID dst_label, VertexID v_src, VertexID v_dst) {
        auto& edges = label_edges[src_label][dst_label];
        if (edges == nullptr) {
            edges = new CSMEdges;
        }
        edges->add_edge(v_src, v_dst);
    }
    inline void delete_edge(LabelID src_label, LabelID dst_label, VertexID v_src, VertexID v_dst) {
        auto edges = label_edges[src_label][dst_label];
        if (edges == nullptr) return; 
        edges->delete_edge(v_src, v_dst);
    }
};

#endif // LU_AUX_H
