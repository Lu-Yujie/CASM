#include "CSMEngine.h"
#include "utils/graphoperations.h"
#include "QueryEngine.h"
#include "timeOp.h"

void CSMEngine::build_automorphism_edges(const Graph *query_graph) {
    // Divide the vertex into disjoint sets based on automorphisms.
    vector<vector<uint32_t>> automorphisms;
    GraphOperations::compute_automorphism(query_graph, automorphisms);

    uint32_t n = query_graph->getVerticesCount();
    spp::sparse_hash_set<Edge> selected;

    for (uint32_t u = 0; u < n; ++u) {
        uint32_t u_nbr_count;
        auto u_nbr = query_graph->getVertexNeighbors(u, u_nbr_count);
        auto u_label = query_graph->getVertexLabel(u);

        for (uint32_t i = 0; i < u_nbr_count; ++i) {
            auto uu = u_nbr[i];
            auto uu_label = query_graph->getVertexLabel(uu);
            Edge e(u, uu, u_label, uu_label);

            if (!selected.contains(e)) {
                selected.insert(e);
                automorphism_edges_.push_back({e});
                for (auto &embedding: automorphisms) {
                    auto v = embedding[u];
                    auto vv = embedding[uu];
                    auto v_label = query_graph->getVertexLabel(v);
                    auto vv_label = query_graph->getVertexLabel(vv);
                    Edge mapped_e(v, vv, v_label, vv_label);
                    if (!selected.contains(mapped_e)) {
                        selected.insert(mapped_e);
                        automorphism_edges_.back().push_back(mapped_e);
                    }
                }
            }
        }
    }
}

void CSMEngine::build_edges_mapping(const Graph *query_graph) {
    for (size_t i = 0; i < automorphism_edges_.size(); i++) {
        auto& automorphism = automorphism_edges_[i];
        Edge edge = automorphism.front();
        ELabel eLabel = edge.elabel_;
        {
            auto it = label_edge_mapping_.find(eLabel);
            if (it == label_edge_mapping_.end()) {
                auto temp_it = label_edge_mapping_.emplace(eLabel, std::vector<Edge>());
                it = temp_it.first;
            }
            for (auto e: automorphism) it->second.push_back(e);
        }
        {
            auto it = label_automorphism_mapping_.find(eLabel);
            if (it == label_automorphism_mapping_.end()) {
                auto temp_it = label_automorphism_mapping_.emplace(eLabel, std::vector<uint32_t>());
                it = temp_it.first;
            }
            it->second.push_back(i);
        }
    }
}

// 构建 global CSMIndex, 构建 automorphism information
void CSMEngine::init(const Graph *data_graph, const Graph *query_graph) {
    csm_index = new CSMIndex;
    csm_index->init(query_graph, data_graph);
    build_automorphism_edges(query_graph);
    build_edges_mapping(query_graph);
}

// 函数签名修改：添加 QueryStats& stats
void CSMEngine::query(Update de, size_t output_limit, mpz_t embedding_cnt, int64_t& end_time, QueryStats& stats) {
    // 1. 第一步：先收集所有受到该数据边影响的查询边 (matched_edges)
    vector<Edge> matched_edges;
    for (auto& edge_group : automorphism_edges_) {
        if (edge_group[0].elabel_ == de.edge_.elabel_) {
            for (auto& edge : edge_group) {
                matched_edges.emplace_back(edge);
            }
        }
    }

    // 2. 第二步：如果是插入操作 (+)，必须先更新 Global Index
    if (de.op_ == '+') {
        auto t_start = QueryStats::now(); // [TIMER START]
        csm_index->update_Aux(de, matched_edges);
        auto t_end = QueryStats::now();   // [TIMER END]
        QueryStats::add_duration(stats.time_update_aux_ns, t_start, t_end);
    }

    // 3. 第三步：执行查询
    mpz_t one_embedding_cnt;
    mpz_init(one_embedding_cnt);
    for (size_t i = 0; i < automorphism_edges_.size(); i++) {
        auto& edge_group = automorphism_edges_[i];

        if (edge_group[0].elabel_ == de.edge_.elabel_) {
            // --- 统计 try_build_local ---
            auto t_build_start = QueryStats::now();
            bool build_success = csm_index->try_build_local(de.edge_, edge_group[0]);
            auto t_build_end = QueryStats::now();
            QueryStats::add_duration(stats.time_try_build_ns, t_build_start, t_build_end);
            // ---------------------------

            if (build_success) {
                mpz_set_ui(one_embedding_cnt, 0);

                // --- 统计 Candidates 数量 ---
                // 假设 candidates_count_ptr 是一个数组，长度为查询图的顶点数
                // 如果您有特定的获取顶点数的方法，请在此处替换 global_index->aux.query_graph->getVerticesCount()
                size_t q_v_num = csm_index->mem->q_graph->getVerticesCount(); 
                unsigned long long current_cands = 0;
                for(size_t v_idx = 0; v_idx < q_v_num; ++v_idx) {
                    current_cands += csm_index->cans[v_idx].size();
                }
                stats.total_candidates += current_cands;
                stats.search_invocations++;
                if (current_cands > stats.max_candidates) stats.max_candidates = current_cands;
                if (current_cands < stats.min_candidates) stats.min_candidates = current_cands;
                // ---------------------------

                // --- 统计 BSXEngine ---
                auto t_bsx_start = QueryStats::now();
                // QueryEngine::BSXEngine(global_index->aux.dnum,
                //                        global_index->aux.query_graph,
                //                        adapter.edge_matrix_ptr,
                //                        adapter.candidates_ptr,
                //                        adapter.candidates_count_ptr,
                //                        global_index->mem->order,
                //                        output_limit, one_embedding_cnt, end_time);
                QueryEngine::QuickEngine(csm_index, output_limit, one_embedding_cnt, end_time);
                auto t_bsx_end = QueryStats::now();
                QueryStats::add_duration(stats.time_bsx_ns, t_bsx_start, t_bsx_end);
                // ---------------------

                // --- 统计 零结果 (Zero Results) ---
                if (mpz_cmp_ui(one_embedding_cnt, 0) == 0) {
                    stats.bsx_zero_count++;
                }

                mpz_mul_ui(one_embedding_cnt, one_embedding_cnt, edge_group.size());
                mpz_add(embedding_cnt, embedding_cnt, one_embedding_cnt);

                if (TimeOp::getClockNan() >= end_time) break;
            }
        }
    }
    mpz_clear(one_embedding_cnt);

    // 4. 第四步：如果是删除操作 (-)，更新 Global Index
    if (de.op_ == '-') {
        auto t_start = QueryStats::now(); // [TIMER START]
        csm_index->update_Aux(de, matched_edges);
        auto t_end = QueryStats::now();   // [TIMER END]
        QueryStats::add_duration(stats.time_update_aux_ns, t_start, t_end);
    }
}
