#include "BuildAux.h"
#include <algorithm>
using namespace std;

template <typename CallbackFunc>
void buildAuxImpl(ui dnum,
                  CallbackFunc get_neighbors,
                  const Graph *query_graph, 
                  vector<vector<VertexID>>& cans,
                  CSMEdges ***edge_matrix) {

    ui q_num = query_graph->getVerticesCount();
    ui* flag = new ui[dnum];
    ui* updated_flag = new ui[dnum];
    std::fill(flag, flag + dnum, 0);

    for (ui i = 0; i < q_num; ++i) {
        for (ui j = 0; j < q_num; ++j) {
            edge_matrix[i][j] = nullptr;
        }
    }

    std::vector<VertexID> build_table_order(q_num);
    for (ui i = 0; i < q_num; ++i) {
        build_table_order[i] = i;
    }

    std::sort(build_table_order.begin(), build_table_order.end(), [query_graph](VertexID l, VertexID r) {
        if (query_graph->getVertexDegree(l) == query_graph->getVertexDegree(r)) {
            return l < r;
        }
        return query_graph->getVertexDegree(l) > query_graph->getVertexDegree(r);
    });

    for (auto u : build_table_order) {
        ui u_nbrs_count;
        const VertexID* u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_count);
        ui updated_flag_count = 0;

        for (ui i = 0; i < u_nbrs_count; ++i) {
            auto& u_nbr = u_nbrs[i];
            if (edge_matrix[u][u_nbr] != nullptr)
                continue;

            if (updated_flag_count == 0) {
                for (ui j = 0; j < cans[u].size(); ++j) {
                    VertexID v = cans[u][j];
                    flag[v] = j + 1;
                    updated_flag[updated_flag_count++] = v;
                }
            }

            edge_matrix[u_nbr][u] = new CSMEdges;
            edge_matrix[u][u_nbr] = new CSMEdges;
            edge_matrix[u_nbr][u]->edge_.resize(cans[u_nbr].size());
            edge_matrix[u][u_nbr]->edge_.resize(cans[u].size());

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

        for (ui i = 0; i < updated_flag_count; ++i) {
            VertexID v = updated_flag[i];
            flag[v] = 0;
        }
    }
    delete[] flag;
    delete[] updated_flag;
}

void BuildAux::buildAux(
    ui dnum,
    std::function<const VertexID*(VertexID, VertexID, VertexID, ui&)> get_neighbors,
    const Graph *query_graph, 
    vector<vector<VertexID>>& cans,
    CSMEdges ***edge_matrix) 
{
    buildAuxImpl(dnum, get_neighbors, query_graph, cans, edge_matrix);
}

void BuildAux::buildAux(
    ui dnum,
    std::function<const vector<VertexID>&(VertexID, VertexID, VertexID, ui&)> get_neighbors,
    const Graph *query_graph, 
    vector<vector<VertexID>>& cans,
    CSMEdges ***edge_matrix) 
{
    buildAuxImpl(dnum, get_neighbors, query_graph, cans, edge_matrix);
}