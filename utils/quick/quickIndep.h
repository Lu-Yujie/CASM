#ifndef QUICK_INDEP_H
#define QUICK_INDEP_H

#include "graph/graph.h"
#include "gmp.h"
#include <vector>

class QuickIndep {
    // Structure to store context for each fixed edge
    struct EdgeIndepInfo {
        VertexID* cover_set;
        VertexID* indep_set;
        ui cover_num;
        ui indep_num;
    };
    // Maps an undirected edge (min_u, max_v) to its specific independent set context
    std::map<std::pair<VertexID, VertexID>, EdgeIndepInfo> edge_infos;

    // --- Shared Variables for Enumeration ---
    ui* indep_con_cnt;    // Conflict counters for candidates
    const Graph* query_graph;
    ui *degree;

    ui* enum_idx;
    ui* un_con_cnt;
    bool* phase; // true-> Phase 1 (Conflict), false -> Phase 2 (Un-Conflict)
    ui qnum;

    // Generates an independent set with a specific edge fixed into the cover_set
    void buildIndepForEdge(const Graph* graph, VertexID src, VertexID dst, EdgeIndepInfo& info, bool* visited_u);
    // Initializes the independent sets for all edges in the query graph
    void initAllEdges(bool* visited_u);
    void print();

public:
    // --- Current Active Variables (switched via set_cur) ---
    VertexID* indep_set;
    VertexID* cover_set;
    ui indep_num;
    ui cover_num;
    // Shared Buffer Arrays for Candidates
    const VertexID** enum_cans;
    ui* enum_cans_cnt;
    mpz_t* embedding_level;

    QuickIndep(ui dnum, const Graph* query_graph, bool* visited_u);
    ~QuickIndep();

    // Main enumeration entry point
    void enumeration(bool*visited_v);
    void enum4Parts(bool* visited_v);

    // Context Switcher
    void set_cur(VertexID v_src, VertexID v_dst);

    // Strategy: Select independent nodes by Degree (High -> Low)
    void indepSetOnDegree(const Graph* graph, VertexID *order, bool* visited_u);
    // Strategy: Select independent nodes by Candidate Size (Low -> High), then Degree
    void indepSetOnCans(const Graph* graph, VertexID *order, ui* cans_cnt, bool* visited_u);
    // Strategy: Similar to above, but ignores nodes with degree <= 1
    void indepSetOnCansExcludeDegreeOne(const Graph* graph, VertexID *order, ui* cans_cnt, bool* visited_u);
};

#endif // QUICK_INDEP_H