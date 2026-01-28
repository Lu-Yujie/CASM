#include "CSMEngine.h"
#include "utils/graphoperations.h"
#include "EvaluateQuery.h"
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
    global_index = new CSMIndex;
    local_index = new CSMIndex;
    global_index->build_Aux(data_graph, query_graph);
    local_index->aux.init(query_graph);
    build_automorphism_edges(query_graph);
    build_edges_mapping(query_graph);
}

void CSMEngine::query(Update de, size_t output_limit, mpz_t embedding_cnt, int64_t& end_time) {
    // 1. 第一步：先收集所有受到该数据边影响的查询边 (matched_edges)
    // 这一步必须在更新或查询之前完成
    vector<Edge> matched_edges;
    for (auto& edge_group : automorphism_edges_) {
        // 如果当前同构组的边的 label 与数据边一致，则该组所有边都需要更新索引
        if (edge_group[0].elabel_ == de.edge_.elabel_) {
            for (auto& edge : edge_group) {
                matched_edges.emplace_back(edge);
            }
        }
    }

    // 2. 第二步：如果是插入操作 (+)，必须先更新 Global Index
    // 这样 try_build_local 和 BSXEngine 才能在索引中“看到”这条新边
    if (de.op_ == '+') {
        // 注意：这里调用的是上一轮修复后的 update_edge (支持 +/-)
        global_index->update_Aux(de, matched_edges);
    }

    // 3. 第三步：执行查询
    // 此时无论是插入还是删除，Global Index 的状态都包含了这条边，可以被搜索到
    mpz_t one_embedding_cnt;
    mpz_init(one_embedding_cnt);
    for (size_t i = 0; i < automorphism_edges_.size(); i++) {
        auto& edge_group = automorphism_edges_[i];

        // 只处理 Label 匹配的组
        if (edge_group[0].elabel_ == de.edge_.elabel_) {
            // 尝试基于当前数据边构建局部搜索环境
            if (local_index->try_build_local(global_index, de.edge_, edge_group[0])) {
                mpz_set_ui(one_embedding_cnt, 0);

                // 执行回溯搜索
                QueryAdapter adapter(local_index->aux);
                EvaluateQuery::BSXEngine(global_index->aux.dnum,
                                         global_index->aux.query_graph,
                                         adapter.edge_matrix_ptr,
                                         adapter.candidates_ptr,
                                         adapter.candidates_count_ptr,
                                         global_index->pruneCache->order,
                                         output_limit, one_embedding_cnt, end_time);

                // 利用同构性质，乘以组的大小
                mpz_mul_ui(one_embedding_cnt, one_embedding_cnt, edge_group.size());
                mpz_add(embedding_cnt, embedding_cnt, one_embedding_cnt);
                
                if (TimeOp::getClockNan() >= end_time) break;
            }
        }
    }
    mpz_clear(one_embedding_cnt);

    // 4. 第四步：如果是删除操作 (-)，在查询完成后更新 Global Index
    // 之前索引里有这条边，查完了现在把它删掉
    if (de.op_ == '-') {
        global_index->update_Aux(de, matched_edges);
    }
}
