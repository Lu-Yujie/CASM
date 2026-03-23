#include "aux.h"
#include <algorithm>
#include <cstring>

using namespace std;

Aux::~Aux() {
    for (auto& row : label_edges) {
        for (auto* edges : row) {
            if (edges != nullptr) {
                delete edges;
            }
        }
    }
}

void Aux::buildData(const Graph* query_graph, const Graph* data_graph, uint64_t* visited) {
    auto q_num = query_graph->getVerticesCount();
    ui vlabels_count = data_graph->getLabelsCount(); 

    // 1. 缓存查询图顶点 Label，供 getNeighbors 加速映射
    q_labels.resize(q_num);
    for (ui u = 0; u < q_num; u++) {
        q_labels[u] = query_graph->getVertexLabel(u);
    }

    // 2. 初始化本地全量索引矩阵
    label_edges.resize(vlabels_count);
    for (ui i = 0; i < vlabels_count; ++i) {
        label_edges[i].assign(vlabels_count, nullptr);
    }

    // 3. 全量深拷贝 Data Graph 的 data_edge_index_
    for (ui i = 0; i < vlabels_count; ++i) {
        for (ui j = 0; j < vlabels_count; ++j) {
            const CSMEdges* base_edges = data_graph->getDataEdgeIndex(i, j);
            if (base_edges != nullptr) {
                label_edges[i][j] = new CSMEdges(*base_edges); 
            }
        }
    }

    std::memset(visited, 0, sizeof(uint64_t) * 64);
}
