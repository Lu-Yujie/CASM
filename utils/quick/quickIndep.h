#ifndef QUICK_INDEP_H
#define QUICK_INDEP_H

#include "graph/graph.h"
#include "gmp.h"
#include <vector>

class QuickIndep {
    ui* indep_con_cnt;    // Conflict counters for candidates
    const Graph* query_graph;
    ui *degree;

    ui* enum_idx;
    ui* un_con_cnt;
    bool* phase; // true-> Phase 1 (Conflict), false -> Phase 2 (Un-Conflict)

public:
    mpz_t* embedding_level;
    VertexID* indep_set;
    VertexID* cover_set;
    const VertexID** enum_cans;
    ui* enum_cans_cnt;

    ui indep_num;
    ui cover_num;

    QuickIndep(ui dnum, const Graph* query_graph, bool* visited_u);
    ~QuickIndep();

    // Main enumeration entry point
    void enumeration(bool*visited_v);

    void enum4Parts(bool* visited_v);

    // Strategy: Select independent nodes by Degree (High -> Low)
    // can be used for all updates
    void indepSetOnDegree(const Graph* graph, VertexID *order, bool* visited_u);

    // Strategy: Select independent nodes by Candidate Size (Low -> High), then Degree
    void indepSetOnCans(const Graph* graph, VertexID *order, ui* cans_cnt, bool* visited_u);

    // Strategy: Similar to above, but ignores nodes with degree <= 1
    void indepSetOnCansExcludeDegreeOne(const Graph* graph, VertexID *order, ui* cans_cnt, bool* visited_u);
};

#endif // QUICK_INDEP_H