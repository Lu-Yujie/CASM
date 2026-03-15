#ifndef CSM_ENGINE_H
#define CSM_ENGINE_H
#include "CSMIndex.h"
#include "gmp.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip> // for std::fixed, std::setprecision

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