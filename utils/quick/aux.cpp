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
        for (ui i = 0; i < q_num; i++) {
            for (ui j = 0; j < q_num; j++) {
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

void Aux::buildData(const Graph* query_graph, const Graph* data_graph, uint64_t* visited) {
    // init data
    q_num = query_graph->getVerticesCount();
    data = new CSMEdges **[q_num];
    for (ui u = 0; u < q_num; u++) {
        data[u] = new CSMEdges*[q_num];
        memset(data[u], 0, sizeof(CSMEdges*)* q_num);
        ui u_nbrs_count;
        auto u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_count);
        for (ui i = 0; i < u_nbrs_count; ++i) {
            auto& u_nbr = u_nbrs[i];
            data[u][u_nbr] = new CSMEdges;
        }
    }

    std::memset(visited, 0, sizeof(uint64_t) * 64);

    for (ui u = 0; u < q_num; u++) {
        auto ulabel = query_graph->getVertexLabel(u);
        ui u_nbrs_count;
        const VertexID* u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_count);

        for (ui i = 0; i < u_nbrs_count; ++i) {
            auto& u_nbr = u_nbrs[i];
            auto unbr_label = query_graph->getVertexLabel(u_nbr);

            if (visited[u] & (1ULL << u_nbr)) continue;
            visited[u] |= (1ULL << u_nbr);
            visited[u_nbr] |= (1ULL << u);

            CSMEdges* edges_u_unbr = data[u][u_nbr];
            CSMEdges* edges_unbr_u = data[u_nbr][u];

            ui vlabel_vs_cnt = 0;
            auto vlabel_vs = data_graph->getVerticesByLabel(ulabel, vlabel_vs_cnt);

            for (ui v_idx = 0; v_idx < vlabel_vs_cnt; v_idx++) {
                VertexID v = vlabel_vs[v_idx];

                ui v_nbrs_count = 0;
                const VertexID* v_nbrs_ptr = data_graph->getVertexNeighbors(v, v_nbrs_count);
                if (v_nbrs_ptr == nullptr || v_nbrs_count == 0) continue;

                for (ui k = 0; k < v_nbrs_count; ++k) {
                    VertexID v_nbr = v_nbrs_ptr[k];
                    if (data_graph->getVertexLabel(v_nbr) != unbr_label) continue;

                    edges_u_unbr->edge_map[v].push_back(v_nbr);
                    edges_unbr_u->edge_map[v_nbr].push_back(v);
                }
            }

            for (auto& pair : edges_u_unbr->edge_map) {
                std::sort(pair.second.begin(), pair.second.end());
            }
            for (auto& pair : edges_unbr_u->edge_map) {
                std::sort(pair.second.begin(), pair.second.end());
            }
        }
    }
}
