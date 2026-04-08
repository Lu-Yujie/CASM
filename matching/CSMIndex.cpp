#include "CSMIndex.h"
#include "utils/setOp.h"
#include "utils/bSearch.h"
#include <set>
#include "assert.h"
using namespace std;

void CSMIndex::init(const Graph *query_graph, const Graph *data_graph) {
    auto q_num = query_graph->getVerticesCount();
    auto d_num = data_graph->getVerticesCount();
    auto max_cans = data_graph->getGraphMaxLabelFrequency();
    cans.resize(q_num);
    for (ui u = 0; u < q_num; u++) {
        cans[u].reserve(max_cans);
    }
    mem = new MemoryManager(max_cans, query_graph, data_graph);
    aux.buildData(query_graph, data_graph, mem->visited_bitmask);
    csm_filter.init(data_graph);
    isei.init(d_num, data_graph->getOffsets(), data_graph->getEdges());
}

void CSMIndex::update_Aux(Update& de) {
    auto& d_num = mem->d_num;

    auto v_src = de.edge_.src();
    auto v_dst = de.edge_.dst();
    char op = de.op_;
    LabelID src_label = de.src_label();
    LabelID dst_label = de.dst_label();

    // update filter
    csm_filter.update_filter(de);
    isei.stageEdgeUpdate(v_src, v_dst);

    if (op == '+') {  // === 插入操作 ===
        if (v_src >= d_num) d_num = v_src + 1;
        if (v_dst >= d_num) d_num = v_dst + 1;
        aux.insert_edge(src_label, dst_label, v_src, v_dst);
        aux.insert_edge(dst_label, src_label, v_dst, v_src); 
    } else {  // === 删除操作 ===
        aux.delete_edge(src_label, dst_label, v_src, v_dst);
        aux.delete_edge(dst_label, src_label, v_dst, v_src);
    }
}

// --- 辅助：种子点约束传播 (Seed Propagation) ---
bool CSMIndex::propagate_neighbor_constraint(ui u_fixed, VertexID v_fixed, uint64_t& in_queue) {
    auto& query_graph = mem->q_graph;
    auto& local_cans = this->cans;
    auto& visited = mem->visited_bitmask[0];
    auto& m_heap = mem->m_heap;

    ui unbr_cnt = 0;
    const VertexID* unbrs = query_graph->getVertexNeighbors(u_fixed, unbr_cnt);

    for (ui i = 0; i < unbr_cnt; ++i) {
        ui unbr = unbrs[i];

        const vector<VertexID>& valid_candidates = aux.getNeighbors(u_fixed, unbr, v_fixed);
        if (valid_candidates.empty()) return false;

        if (!get_bit(visited, unbr)) {
            ui unbr_q_deg = query_graph->getVertexDegree(unbr);
            auto& target_cans = local_cans[unbr];

            target_cans.clear();

            for (VertexID v : valid_candidates) {
                if (csm_filter.filter_check(v, unbr_q_deg, aux.q_labels[unbr])) {
                    target_cans.push_back(v);
                }
            }

            if (target_cans.empty()) return false;

            set_bit(visited, unbr);
            set_bit(in_queue, unbr);
            m_heap.push(unbr, target_cans.size());
        } else {
            ui old_size = local_cans[unbr].size();
            SetOp::intersectAndUpdate(local_cans[unbr], valid_candidates);
            if (local_cans[unbr].empty()) return false;

            // 应用阈值：只有缩小显著，才重新入队
            if (SIGNIFICANT_DROP(old_size, local_cans[unbr].size()) && !get_bit(in_queue, unbr)) {
                set_bit(in_queue, unbr);
                m_heap.push(unbr, local_cans[unbr].size());
            }
        }
    }
    return true;
}

// --- 通用前向传播 (BFS Step) ---
bool CSMIndex::propagate_forward(ui u, ui unbr) {
    auto& local_cans = this->cans;
    auto& flag_array = mem->flag_array;
    auto& reset_buffer = mem->reset_buffer;
    auto& d_num = mem->d_num;
    auto& visited = mem->visited_bitmask[0];

    // 1. 清理 buffer
    reset_buffer.clear(); 

    // 2. Push & Deduplicate
    const auto& u_current_cans = local_cans[u];
    for (VertexID u_can : u_current_cans) {
        const vector<VertexID>& valid_neighbors = aux.getNeighbors(u, unbr, u_can);
        for (VertexID v_nbr : valid_neighbors) {
            if (flag_array[v_nbr] == false) {  // 去重
                flag_array[v_nbr] = true;
                reset_buffer.push_back(v_nbr); 
            }
        }
    }

    if (reset_buffer.empty()) {  // 不需要清理 flag_array
        return false;
    }

    // 3. Update Local Candidates
    if (!get_bit(visited, unbr)) {  // unbr 未访问, 将 reset_buffer 作为新的候选集
        ui unbr_q_deg = mem->q_graph->getVertexDegree(unbr);
        ui write_idx = 0;

        // reset flag_array & complete filter_check
        for (VertexID v : reset_buffer) {
            flag_array[v] = false;

            if (csm_filter.filter_check(v, unbr_q_deg, aux.q_labels[unbr])) {
                reset_buffer[write_idx++] = v;
            }
        }
        
        reset_buffer.resize(write_idx);
        if (reset_buffer.empty()) return false;

        std::sort(reset_buffer.begin(), reset_buffer.end());
        local_cans[unbr].swap(reset_buffer);
    } else {  // unbr 已访问, 过滤现有的 local_cans[unbr]，只保留 flag 为 1 的元素
        auto& target = local_cans[unbr];
        ui write_idx = 0;
        for (ui read_idx = 0; read_idx < target.size(); ++read_idx) {
            VertexID v = target[read_idx];
            if (flag_array[v] == true) {
                target[write_idx++] = v;
            }
        }
        target.resize(write_idx);

        for (auto& v : reset_buffer) flag_array[v] = false;  // clear buffer

        if (target.empty()) return false;
    }

    return true;
}

// --- 构建局部索引主流程 ---
bool CSMIndex::try_build_local(Edge de, Edge qe) {
    auto q_src = qe.src();
    auto q_dst = qe.dst();
    auto v_src = de.src();
    auto v_dst = de.dst();

    auto& q_num = mem->q_num;
    const auto& all_visited = mem->all_visited;
    auto& local_cans = this->cans;
    auto& flag_array = mem->flag_array;
    auto& d_num = mem->d_num;
    auto& visited = mem->visited_bitmask[0];
    
    uint64_t in_queue = 0; 
    visited = 0;
    auto& m_heap = mem->m_heap;
    m_heap.clear();

    local_cans[q_src].clear();
    local_cans[q_src].emplace_back(v_src);
    set_bit(visited, q_src);
    
    local_cans[q_dst].clear();
    local_cans[q_dst].emplace_back(v_dst);
    set_bit(visited, q_dst);

    // 快速剪枝
    if (!propagate_neighbor_constraint(q_src, v_src, in_queue)) return false;
    if (!propagate_neighbor_constraint(q_dst, v_dst, in_queue)) return false;

    // 传播剪枝
    while (!m_heap.empty()) {
        ui u = m_heap.pop();
        clear_bit(in_queue, u);

        if (local_cans[u].empty()) return false;

        ui unbr_cnt = 0;
        const VertexID* unbrs = mem->q_graph->getVertexNeighbors(u, unbr_cnt);

        for (ui i = 0; i < unbr_cnt; ++i) {
            ui unbr = unbrs[i];

            bool was_visited = get_bit(visited, unbr);
            ui old_size = was_visited ? local_cans[unbr].size() : 0;

            // 执行传播
            if (!propagate_forward(u, unbr)) return false;

            // 应用自适应阈值
            if (!was_visited || SIGNIFICANT_DROP(old_size, local_cans[unbr].size())) {
                if (!was_visited) set_bit(visited, unbr);
                
                if (!get_bit(in_queue, unbr)) {
                    set_bit(in_queue, unbr);
                    m_heap.push(unbr, local_cans[unbr].size());
                }
            }
        }
    }

    // 孤立点检查
    if (LIKELY(visited == all_visited)) return true;
    for (ui i = 0; i < q_num; ++i) {
        auto u_label = mem->q_graph->getVertexLabel(i);
        ui vlabel_vs_cnt = 0;
        auto vlabel_vs = mem->d_graph->getVerticesByLabel(u_label, vlabel_vs_cnt);
        if (vlabel_vs_cnt == 0) return false;
        if (!get_bit(visited, i)) {
            local_cans[i].assign(vlabel_vs, vlabel_vs+vlabel_vs_cnt);
        }
    }

    return true;
}
