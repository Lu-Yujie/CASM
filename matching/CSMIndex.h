#ifndef LU_CSMINDEX_H
#define LU_CSMINDEX_H

#include "utils/quick/quickIndex.h"
#include "utils/quick/memoryManager.h"
#include "utils/quick/aux.h"

using namespace std;

class CSMIndex {
public:
    Aux aux;
    static MemoryManager* mem;

    CSMIndex() {}
    ~CSMIndex() {
        if (mem != nullptr) {
            delete mem;
            mem = nullptr;
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
    inline void set_bit(uint64_t& mask, ui idx) { mask |= (1ULL << idx); }
    inline bool get_bit(uint64_t mask, ui idx) { return (mask >> idx) & 1ULL; }
};

#endif
