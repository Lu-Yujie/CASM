#ifndef CSM_GRAPHOPERATIONS_H
#define CSM_GRAPHOPERATIONS_H

#include "graph/graph.h"
class GraphOperations {
public:
    static void compute_automorphism(const Graph* graph, std::vector<std::vector<uint32_t>>& embeddings);
};


#endif //CSM_GRAPHOPERATIONS_H
