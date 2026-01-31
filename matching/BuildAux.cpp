#include "BuildAux.h"
#include <algorithm>
#include <numeric>
using namespace std;

template <typename CallbackFunc>
void buildAuxImpl(ui dnum,
                  CallbackFunc get_neighbors,
                  const Graph *query_graph, 
                  vector<vector<VertexID>>& cans,
                  CSMEdges ***edge_matrix,
                  vector<ui>& flag,
                  vector<ui>& updated_flag,
                  uint64_t* visited) {
    ui q_num = query_graph->getVerticesCount();
    std::memset(visited, 0, sizeof(uint64_t) * 64);

    // 用于保留 capacity 的深度清理, 嵌套 vector 做外层resize可能会释放内层 vector 的内存
    auto reset_edge_vec = [](std::vector<std::vector<VertexID>>& edges, size_t new_size) {
        if (edges.size() < new_size) {
            edges.resize(new_size);
        }
        for (size_t k = 0; k < new_size; ++k) {
            edges[k].clear(); 
        }
    };

    for (ui u = 0; u < q_num; u++) {
        ui u_nbrs_count;
        const VertexID* u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_count);
        updated_flag.clear();
        for (ui j = 0; j < cans[u].size(); ++j) {
            VertexID v = cans[u][j];
            flag[v] = j + 1;
            updated_flag.push_back(v);
        }

        for (ui i = 0; i < u_nbrs_count; ++i) {
            auto& u_nbr = u_nbrs[i];
            if (visited[u] & (1ULL << u_nbr)) {
                continue;
            }
            visited[u] |= (1ULL << u_nbr);
            visited[u_nbr] |= (1ULL << u);

            CSMEdges* fwd_ptr = edge_matrix[u_nbr][u];
            CSMEdges* bwd_ptr = edge_matrix[u][u_nbr];
            reset_edge_vec(fwd_ptr->edge_, cans[u_nbr].size());
            reset_edge_vec(bwd_ptr->edge_, cans[u].size());

            for (ui j = 0; j < cans[u_nbr].size(); ++j) {
                VertexID v = cans[u_nbr][j];
                ui v_nbrs_count = 0;

                const auto& result = get_neighbors(u_nbr, u, v, v_nbrs_count);
                using ResultType = typename std::decay<decltype(result)>::type;
                const VertexID* v_nbrs_ptr = nullptr;

                if constexpr (std::is_pointer_v<ResultType>) {
                    v_nbrs_ptr = result;
                }
                else {
                    v_nbrs_ptr = result.data();
                    v_nbrs_count = result.size();
                }

                if (v_nbrs_ptr == nullptr || v_nbrs_count == 0) continue;

                for (ui k = 0; k < v_nbrs_count; ++k) {
                    VertexID v_nbr = v_nbrs_ptr[k];
                    if (flag[v_nbr] != 0) {
                        ui u_idx = flag[v_nbr] - 1;

                        // 添加正向边: u_nbr[j] -> v_nbr
                        edge_matrix[u_nbr][u]->edge_[j].push_back(v_nbr);
                        // 添加反向边: u[u_idx] -> v, 无需 sort
                        edge_matrix[u][u_nbr]->edge_[u_idx].push_back(v);
                    }
                }
            }
        }

        for (auto& v : updated_flag) flag[v] = 0;
    }
}

void BuildAux::buildAux(
    ui dnum,
    std::function<const VertexID*(VertexID, VertexID, VertexID, ui&)> get_neighbors,
    const Graph *query_graph, 
    vector<vector<VertexID>>& cans,
    CSMEdges ***edge_matrix,
    vector<ui>& flag,
    vector<ui>& updated_flag,
    uint64_t* visited)
{
    buildAuxImpl(dnum, get_neighbors, query_graph, cans, edge_matrix, flag, updated_flag, visited);
}

void BuildAux::buildAux(
    ui dnum,
    std::function<const vector<VertexID>&(VertexID, VertexID, VertexID, ui&)> get_neighbors,
    const Graph *query_graph, 
    vector<vector<VertexID>>& cans,
    CSMEdges ***edge_matrix,
    vector<ui>& flag,
    vector<ui>& updated_flag,
    uint64_t* visited)
{
    buildAuxImpl(dnum, get_neighbors, query_graph, cans, edge_matrix, flag, updated_flag, visited);
}