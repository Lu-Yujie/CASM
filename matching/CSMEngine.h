#ifndef CSM_ENGINE_H
#define CSM_ENGINE_H
#include "CSMIndex.h"
#include "gmp.h"

struct QueryAdapter {
    // === 适配后的数据，供 BSXEngine 直接使用 ===
    ui** candidates_ptr = nullptr;     // 适配 ui** candidates
    ui* candidates_count_ptr = nullptr;// 适配 ui* candidates_count
    Edges*** edge_matrix_ptr = nullptr;// 适配 Edges*** edge_matrix
    
    // === 内部持有资源的容器 ===
    ui qnum;
    std::vector<ui*> cans_ptrs_holder; // 持有指针数组
    std::vector<ui> cans_counts_holder;// 持有计数数组

    // 构造函数：执行转换
    QueryAdapter(Aux& local_aux) {
        qnum = local_aux.qnum;
        // --- 1. 适配 Candidates ---
        cans_ptrs_holder.resize(qnum);
        cans_counts_holder.resize(qnum);
        for (ui i = 0; i < qnum; ++i) {
            // vector::data() 返回数组首地址，兼容 ui*
            cans_ptrs_holder[i] = local_aux.cans[i].data();
            cans_counts_holder[i] = local_aux.cans[i].size();
        }
        candidates_ptr = cans_ptrs_holder.data();
        candidates_count_ptr = cans_counts_holder.data();

        // --- 2. 适配 Edge Matrix (Vector -> CSR) ---
        // 这是最耗时的部分，但由于局部图很小，通常非常快
        edge_matrix_ptr = new Edges**[qnum];
        for (ui i = 0; i < qnum; ++i) {
            edge_matrix_ptr[i] = new Edges*[qnum];
            for (ui j = 0; j < qnum; ++j) {
                CSMEdges* csm_edge = local_aux.data[i][j];

                if (csm_edge == nullptr) {
                    edge_matrix_ptr[i][j] = nullptr;
                } else {
                    // 创建旧版 CSR Edges 对象
                    Edges* legacy_edge = new Edges(); 
                    convert_to_csr(csm_edge, legacy_edge);
                    edge_matrix_ptr[i][j] = legacy_edge;
                }
            }
        }
    }

    // 析构函数：自动清理临时创建的 CSR 结构
    ~QueryAdapter() {
        if (edge_matrix_ptr != nullptr) {
            for (ui i = 0; i < qnum; ++i) {
                for (ui j = 0; j < qnum; ++j) {
                    if (edge_matrix_ptr[i][j] != nullptr) {
                        // 释放 CSR 内部数组 (假设 Edges 析构函数会 delete offset/edge)
                        delete edge_matrix_ptr[i][j]; 
                    }
                }
                delete[] edge_matrix_ptr[i];
            }
            delete[] edge_matrix_ptr;
        }
    }

private:
    // 核心转换逻辑：vector<vector> -> CSR
    void convert_to_csr(CSMEdges* src, Edges* dst) {
        auto& vec_data = src->edge_;
        ui rows = vec_data.size();

        dst->vertex_count_ = rows;

        // 1. 计算边总数
        size_t total_edges = 0;
        for (const auto& row : vec_data) {
            total_edges += row.size();
        }
        dst->edge_count_ = total_edges;

        // 2. 分配 CSR 内存
        dst->offset_ = new ui[rows + 1];
        dst->edge_ = new ui[total_edges];

        // 3. 填充数据 (Flatten)
        ui current_offset = 0;
        for (ui i = 0; i < rows; ++i) {
            dst->offset_[i] = current_offset;
            const auto& row = vec_data[i];
            // 内存拷贝：将 vector 数据复制到数组
            if (!row.empty()) {
                std::memcpy(dst->edge_ + current_offset, row.data(), row.size() * sizeof(VertexID));
                current_offset += row.size();
            }
        }
        dst->offset_[rows] = current_offset;
    }
};

#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip> // for std::fixed, std::setprecision

// 用于统计的辅助结构体
struct QueryStats {
    long long time_update_aux_ns = 0;
    long long time_try_build_ns = 0;
    long long time_local_aux_ns = 0;
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
        double t_aux = time_local_aux_ns / 1000000.0;
        double t_build = time_try_build_ns / 1000000.0;
        double t_bsx = time_bsx_ns / 1000000.0;
        double avg_candidates = (search_invocations > 0) ? (double)total_candidates / search_invocations : 0.0;
        unsigned long long actual_min = (search_invocations > 0) ? min_candidates : 0;
        double zero_rate = (search_invocations > 0) ? ((double)bsx_zero_count / search_invocations) * 100.0 : 0.0;

        std::cout << "\n============= Performance Statistics =============" << std::endl;
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "[Timing] update_Aux       : " << t_update << " ms" << std::endl;
        std::cout << "[Timing] local_aux_update : " << t_aux << " ms" << std::endl;
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
    CSMIndex* global_index;
    CSMIndex* local_index;
    vector<vector<Edge>> automorphism_edges_;
    unordered_map<ELabel, vector<Edge>> label_edge_mapping_;
    unordered_map<ELabel, vector<uint32_t>> label_automorphism_mapping_;
public:
    CSMEngine(): global_index(nullptr), local_index(nullptr), label_edge_mapping_({}), label_automorphism_mapping_({}) {}
    ~CSMEngine() {
        delete global_index;
        delete local_index;
    }
    void init(const Graph *data_graph, const Graph *query_graph);
    void query(Update de, size_t output_limit, mpz_t embedding_cnt, int64_t& end_time, QueryStats& stats);

private:
    void build_automorphism_edges(const Graph *query_graph);
    void build_edges_mapping(const Graph *query_graph);
};

#endif