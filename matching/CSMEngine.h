#ifndef CSM_ENGINE_H
#define CSM_ENGINE_H
#include "CSMIndex.h"
#include "gmp.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip> // for std::fixed, std::setprecision

struct QueryAdapter {
    // === 适配后的数据，供 BSXEngine 直接使用 ===
    ui** candidates_ptr = nullptr;
    ui* candidates_count_ptr = nullptr;
    Edges*** edge_matrix_ptr = nullptr;
    ui qnum;

    QueryAdapter(CSMIndex* csm_index) {
        auto query_graph = csm_index->mem->q_graph;
        qnum = query_graph->getVerticesCount();
        ui dnum = csm_index->mem->d_num;
        auto& cans = csm_index->cans;
        auto& aux = csm_index->aux;

        // --- 1. 适配 Candidates ---
        candidates_ptr = new ui*[qnum];
        candidates_count_ptr = new ui[qnum];
        for (ui i = 0; i < qnum; ++i) {
            candidates_ptr[i] = const_cast<ui*>(cans[i].data());
            candidates_count_ptr[i] = cans[i].size();
        }

        // --- 2. 初始化 Edge Matrix ---
        edge_matrix_ptr = new Edges**[qnum];
        for (ui i = 0; i < qnum; ++i) {
            edge_matrix_ptr[i] = new Edges*[qnum];
            for (ui j = 0; j < qnum; ++j) {
                edge_matrix_ptr[i][j] = nullptr;
            }
        }

        // --- 3. 构建局部的 Edge Matrix (核心逻辑) ---
        std::vector<ui> flag(dnum, 0);
        std::vector<VertexID> updated_flag;
        std::vector<uint64_t> visited(qnum, 0); // 防重位图，最多支持 64 个查询点

        for (ui u = 0; u < qnum; u++) {
            ui u_nbrs_count;
            const VertexID* u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_count);

            // 给 u 的候选点打上 flag，值为 candidate_index + 1
            updated_flag.clear();
            for (ui j = 0; j < cans[u].size(); ++j) {
                VertexID v = cans[u][j];
                flag[v] = j + 1;
                updated_flag.push_back(v);
            }

            for (ui i = 0; i < u_nbrs_count; ++i) {
                ui u_nbr = u_nbrs[i];

                // 无向图边只需处理一次，避免双向重复计算
                if (visited[u] & (1ULL << u_nbr)) {
                    continue;
                }
                visited[u] |= (1ULL << u_nbr);
                visited[u_nbr] |= (1ULL << u);

                std::vector<std::vector<VertexID>> fwd_temp(cans[u_nbr].size()); // u_nbr -> u
                std::vector<std::vector<VertexID>> bwd_temp(cans[u].size());     // u -> u_nbr

                for (ui j = 0; j < cans[u_nbr].size(); ++j) {
                    VertexID v = cans[u_nbr][j];
                    const auto& v_nbrs = aux.getNeighbors(u_nbr, u, v);

                    for (VertexID v_nbr : v_nbrs) {
                        if (flag[v_nbr] != 0) {
                            ui u_idx = flag[v_nbr] - 1;
                            fwd_temp[j].push_back(v_nbr);
                            bwd_temp[u_idx].push_back(v);
                        }
                    }
                }
                edge_matrix_ptr[u_nbr][u] = convert_to_csr(fwd_temp);
                edge_matrix_ptr[u][u_nbr] = convert_to_csr(bwd_temp);
            }
            for (auto& v : updated_flag) {
                flag[v] = 0;
            }
        }
    }

    ~QueryAdapter() {
        delete[] candidates_ptr;
        delete[] candidates_count_ptr;

        if (edge_matrix_ptr != nullptr) {
            for (ui i = 0; i < qnum; ++i) {
                for (ui j = 0; j < qnum; ++j) {
                    if (edge_matrix_ptr[i][j] != nullptr) {
                        delete edge_matrix_ptr[i][j];
                    }
                }
                delete[] edge_matrix_ptr[i];
            }
            delete[] edge_matrix_ptr;
        }
    }

private:
    // 转换为 BSXEngine 专用的 CSR 结构
    Edges* convert_to_csr(std::vector<std::vector<VertexID>>& src_edges) {
        Edges* dst = new Edges();
        ui row_count = src_edges.size();
        dst->vertex_count_ = row_count; // 行数即当前查询点的 candidate 数量

        size_t total_edges = 0;
        for (ui i = 0; i < row_count; ++i) {
            total_edges += src_edges[i].size();
        }
        dst->edge_count_ = total_edges;

        // offset 大小为 candidates_count + 1
        dst->offset_ = new ui[row_count + 1];
        dst->edge_ = new ui[total_edges];

        ui current_offset = 0;
        for (ui i = 0; i < row_count; ++i) {
            dst->offset_[i] = current_offset;
            if (!src_edges[i].empty()) {
                std::memcpy(dst->edge_ + current_offset, src_edges[i].data(), src_edges[i].size() * sizeof(VertexID));
                current_offset += src_edges[i].size();
            }
        }
        dst->offset_[row_count] = current_offset;
        
        return dst;
    }
};

// 用于统计的辅助结构体
struct QueryStats {
    long long time_update_aux_ns = 0;
    long long time_try_build_ns = 0;
    long long time_bsx_ns = 0;

    unsigned long long total_candidates = 0;
    unsigned long long max_candidates = 0;
    unsigned long long min_candidates = std::numeric_limits<unsigned long long>::max();
    unsigned long long search_invocations = 0;
    unsigned long long bsx_zero_count = 0;

    // 辅助函数：获取当前时间点
    static std::chrono::high_resolution_clock::time_point now() {
        return std::chrono::high_resolution_clock::now();
    }

    // 辅助函数：计算时间差并累加
    static void add_duration(long long& accumulator, 
                             std::chrono::high_resolution_clock::time_point start, 
                             std::chrono::high_resolution_clock::time_point end) {
        accumulator += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }

    // 打印统计结果
    void print_report() {
        double t_update = time_update_aux_ns / 1000000.0;
        double t_build = time_try_build_ns / 1000000.0;
        double t_bsx = time_bsx_ns / 1000000.0;
        double avg_candidates = (search_invocations > 0) ? (double)total_candidates / search_invocations : 0.0;
        unsigned long long actual_min = (search_invocations > 0) ? min_candidates : 0;
        double zero_rate = (search_invocations > 0) ? ((double)bsx_zero_count / search_invocations) * 100.0 : 0.0;

        std::cout << "\n============= Performance Statistics =============" << std::endl;
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "[Timing] update_Aux       : " << t_update << " ms" << std::endl;
        std::cout << "[Timing] try_build_local  : " << t_build << " ms" << std::endl;
        std::cout << "[Timing] BSXEngine        : " << t_bsx << " ms" << std::endl;
        std::cout << "--------------------------------------------------" << std::endl;
        std::cout << "[Stats]  Total Searches   : " << search_invocations << std::endl;
        std::cout << "[Stats]  Candidates       : Avg " << avg_candidates 
                  << " (Min: " << actual_min << ", Max: " << max_candidates << ")" << std::endl;
        std::cout << "[Stats]  Zero Results     : " << bsx_zero_count << " times (" << zero_rate << "% fail rate)" << std::endl;
        std::cout << "==================================================" << std::endl;
    }
};

// auxiliary structure for CSM
// 更新的数据边可以匹配到多个查询边上，
class CSMEngine {
private:
    CSMIndex* csm_index;
    vector<vector<Edge>> automorphism_edges_;
    unordered_map<ELabel, vector<Edge>> label_edge_mapping_;
    unordered_map<ELabel, vector<uint32_t>> label_automorphism_mapping_;
public:
    CSMEngine(): csm_index(nullptr), label_edge_mapping_({}), label_automorphism_mapping_({}) {}
    ~CSMEngine() {
        delete csm_index;
    }
    void init(const Graph *data_graph, const Graph *query_graph);
    void query(Update de, size_t output_limit, mpz_t embedding_cnt, int64_t& end_time, QueryStats& stats);

private:
    void build_automorphism_edges(const Graph *query_graph);
    void build_edges_mapping(const Graph *query_graph);
};

#endif