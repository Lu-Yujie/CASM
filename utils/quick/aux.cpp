#include "aux.h"
#include "utils/bSearch.h"
#include <algorithm>
#include <cstring>
#include <numeric>
#include <type_traits>
#include <cassert>

using namespace std;

Aux::~Aux() {
    if (data != nullptr) {
        for (ui i = 0; i < qnum; i++) {
            for (ui j = 0; j < qnum; j++) {
                if (data[i][j]) delete data[i][j];
            }
            delete[] data[i];
        }
        delete[] data;
    }
}

const vector<VertexID>& Aux::getNeighbors(VertexID u_1, VertexID u_2, VertexID v) const {
    auto& edges = *(data[u_1][u_2]);
    auto it = edges.edge_map.find(v);
    if (it != edges.edge_map.end()) {
        return it->second;
    }
    return empty;
}

void Aux::init(const Graph* query_graph, ui max_cans) {
    qnum = query_graph->getVerticesCount();
    this->query_graph = query_graph;
    data = new CSMEdges **[qnum];
    cans.resize(qnum);
    for (ui u = 0; u < qnum; u++) {
        cans[u].reserve(max_cans);
        data[u] = new CSMEdges*[qnum];
        memset(data[u], 0, sizeof(CSMEdges*)* qnum);
        ui u_nbrs_count;
        auto u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_count);
        for (ui i = 0; i < u_nbrs_count; ++i) {
            auto& u_nbr = u_nbrs[i];
            data[u][u_nbr] = new CSMEdges;
        }
    }
}

void Aux::buildData(const Graph* data_graph, const Graph* query_graph, MemoryManager* mem) {
    dnum = data_graph->getVerticesCount();
    
    auto& flag = mem->flag_array;
    auto& updated_flag = mem->reset_buffer;
    auto visited = mem->visited_bitmask;

    ui q_num = query_graph->getVerticesCount();
    std::memset(visited, 0, sizeof(uint64_t) * 64);

    for (ui u = 0; u < q_num; u++) {
        ui u_nbrs_count;
        const VertexID* u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_count);

        updated_flag.clear();
        for (ui j = 0; j < cans[u].size(); ++j) {
            VertexID v = cans[u][j];
            flag[v] = true;
            updated_flag.push_back(v);
        }

        for (ui i = 0; i < u_nbrs_count; ++i) {
            auto& u_nbr = u_nbrs[i];

            if (visited[u] & (1ULL << u_nbr)) continue;
            visited[u] |= (1ULL << u_nbr);
            visited[u_nbr] |= (1ULL << u);

            CSMEdges* fwd_ptr = data[u_nbr][u];
            CSMEdges* bwd_ptr = data[u][u_nbr];
            
            // 清理旧数据
            fwd_ptr->edge_map.clear();
            bwd_ptr->edge_map.clear();

            for (ui j = 0; j < cans[u_nbr].size(); ++j) {
                VertexID v = cans[u_nbr][j]; // data graph 中的节点 v
                ui v_nbrs_count = 0;

                const VertexID* v_nbrs_ptr = data_graph->getVertexNeighbors(v, v_nbrs_count);
                if (v_nbrs_ptr == nullptr || v_nbrs_count == 0) continue;

                for (ui k = 0; k < v_nbrs_count; ++k) {
                    VertexID v_nbr = v_nbrs_ptr[k];
                    if (flag[v_nbr]) {
                        fwd_ptr->edge_map[v].push_back(v_nbr);
                        bwd_ptr->edge_map[v_nbr].push_back(v);
                    }
                }
            }
        }
        for (auto& v : updated_flag) flag[v] = false;
    }
}
