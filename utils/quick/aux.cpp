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
    auto v_idx = b_search::lower_bound_idx(cans[u_1], v);
    if (v_idx == cans[u_1].size()  || cans[u_1][v_idx] != v) {
        return empty;
    }
    auto& edges = *(data[u_1][u_2]);
    assert(edges.edge_.size() == cans[u_1].size() && "edge_.size() != cans[u1].size() — alignment broken");
    return edges[v_idx];
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

template <typename CallbackFunc>
void Aux::buildAuxInternal(ui current_dnum, CallbackFunc get_neighbors, MemoryManager* mem) {
    vector<ui>& flag = mem->flag_array;
    vector<ui>& updated_flag = mem->reset_buffer;
    uint64_t* visited = mem->visited_bitmask;

    ui q_num = query_graph->getVerticesCount();
    std::memset(visited, 0, sizeof(uint64_t) * 64);

    // 辅助 lambda: 用于保留 capacity 的深度清理
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

            CSMEdges* fwd_ptr = data[u_nbr][u];
            CSMEdges* bwd_ptr = data[u][u_nbr];
            reset_edge_vec(fwd_ptr->edge_, cans[u_nbr].size());
            reset_edge_vec(bwd_ptr->edge_, cans[u].size());

            for (ui j = 0; j < cans[u_nbr].size(); ++j) {
                VertexID v = cans[u_nbr][j]; // data graph 中的节点 v
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

                        // 存储在 fwd_ptr (即 data[u_nbr][u]) 的第 j 个位置
                        fwd_ptr->edge_[j].push_back(v_nbr);

                        // 存储在 bwd_ptr (即 data[u][u_nbr]) 的第 u_idx 个位置
                        bwd_ptr->edge_[u_idx].push_back(v);
                    }
                }
            }
        }

        // 清理 flag
        for (auto& v : updated_flag) flag[v] = 0;
    }
}

void Aux::buildData(const Graph* data_graph, const Graph* query_graph, MemoryManager* mem) {
    dnum = data_graph->getVerticesCount();
    auto getNeighbors_ptr = [&](VertexID, VertexID, VertexID v, ui& count) {
        return data_graph->getVertexNeighbors(v, count);
    };
    buildAuxInternal(dnum, getNeighbors_ptr, mem);
}

void Aux::updateData(const Aux& global, MemoryManager* mem) {
    dnum = global.dnum;
    auto getNeighbors_vec = [&](VertexID u1, VertexID u2, VertexID v, ui& /*count*/) -> const vector<VertexID>& {
        return global.getNeighbors(u1, u2, v);
    };
    buildAuxInternal(dnum, getNeighbors_vec, mem);
}