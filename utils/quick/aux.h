#ifndef LU_AUX_H
#define LU_AUX_H

#include "graph/graph.h"
#include "memoryManager.h"

struct Aux {
    CSMEdges*** data = nullptr;
    vector<vector<VertexID>> cans;
    vector<VertexID> empty;
    const Graph* query_graph = nullptr;
    ui dnum, qnum;

    Aux() {}
    ~Aux();
    void init(const Graph* query_graph, ui max_cans);
    void buildData(const Graph* data_graph, const Graph* query_graph, MemoryManager* mem);
    const vector<VertexID>& getNeighbors(VertexID u_1, VertexID u_2, VertexID v) const;
};

#endif // LU_AUX_H
