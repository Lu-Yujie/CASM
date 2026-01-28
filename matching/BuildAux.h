#ifndef SUBGRAPHMATCHING_BUILDAUX_H
#define SUBGRAPHMATCHING_BUILDAUX_H
#include "graph/graph.h"
using namespace std;

class BuildAux {
public:
    // 情况 1: 回调返回指针 (const VertexID*)
    static void buildAux(
        ui dnum,
        std::function<const VertexID*(VertexID, VertexID, VertexID, ui&)> get_neighbors,
        const Graph *query_graph, 
        vector<vector<VertexID>>& cans,
        CSMEdges ***edge_matrix
    );

    // 情况 2: 回调返回 Vector (vector<VertexID>)
    static void buildAux(
        ui dnum,
        std::function<const vector<VertexID>&(VertexID, VertexID, VertexID, ui&)> get_neighbors,
        const Graph *query_graph, 
        vector<vector<VertexID>>& cans,
        CSMEdges ***edge_matrix
    );
};

#endif
