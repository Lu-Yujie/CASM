#ifndef LU_AUX_H
#define LU_AUX_H

#include "graph/graph.h"
#include "memoryManager.h"

struct Aux {
    CSMEdges*** data = nullptr;
    vector<VertexID> empty;
    ui q_num;

    Aux() {}
    ~Aux();
    void buildData(const Graph* query_graph, const Graph* data_graph, uint64_t* visited);
    const vector<VertexID>& getNeighbors(VertexID u_1, VertexID u_2, VertexID v) const;
};

#endif // LU_AUX_H
