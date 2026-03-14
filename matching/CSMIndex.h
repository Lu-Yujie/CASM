#ifndef LU_CSMINDEX_H
#define LU_CSMINDEX_H

#include "utils/quick/quickIndex.h"
#include "utils/quick/memoryManager.h"
#include "utils/quick/aux.h"

using namespace std;

class CSMIndex {
public:
    Aux aux;
    vector<vector<VertexID>> cans;
    static MemoryManager* mem;

    CSMIndex() {}
    ~CSMIndex() {
        if (mem != nullptr) {
            delete mem;
            mem = nullptr;
        }
    }
    void init_local(const Graph *query_graph, const Graph *data_graph);
    void init_global(const Graph *query_graph, const Graph *data_graph);
    bool try_build_local(const CSMIndex* global, Edge de, Edge qe);
    void update_Aux(Update de, vector<Edge>& matched_edges);

private:
    bool propagate_forward(const CSMIndex* global, ui u, ui unbr);
    bool propagate_neighbor_constraint(const CSMIndex* global, ui u_fixed, VertexID v_fixed, uint64_t& in_queue);
    void insert_edge(CSMEdges* old_edges, VertexID v_src, VertexID v_nbr);
    void delete_edge(CSMEdges* old_edges, VertexID v_src, VertexID v_nbr);
    inline void set_bit(uint64_t& mask, ui idx) { mask |= (1ULL << idx); }
    inline void clear_bit(uint64_t& mask, ui idx) { mask &= ~(1ULL << idx); }
    inline bool get_bit(uint64_t mask, ui idx) { return (mask >> idx) & 1ULL; }
};

#endif
