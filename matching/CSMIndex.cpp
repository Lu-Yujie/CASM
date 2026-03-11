#include "CSMIndex.h"
#include "utils/setOp.h"
#include "utils/bSearch.h"
#include <set>
#include "assert.h"
using namespace std;

MemoryManager* CSMIndex::mem = nullptr;
// 只选择符合要求的边，然后构建辅助数据结构。以边为主要过滤条件，且不做传播剪枝
void CSMIndex::build_Aux(const Graph *data_graph, const Graph *query_graph) {
// 首先构建每个点的candidates，然后构建边
// 之后可以做优化，直接识别所有查询边可以匹配的数据边，然后获取每个查询点的带有重复的 cans, 然后对每个点的candidates做重排序。
// 或者可以直接按照边来做查询。
    auto& dnum = aux.dnum;
    auto& qnum = aux.qnum;
    auto& cans = aux.cans;
    dnum = data_graph->getVerticesCount();
    qnum = query_graph->getVerticesCount();
    auto max_cans = data_graph->getGraphMaxLabelFrequency();
    mem = new MemoryManager(max_cans, qnum, dnum, query_graph);
    aux.init(query_graph, max_cans);

    // 构建 cans
    ui unbrs_cnt = 0;
    ui vlabel_vs_cnt = 0;
    ui vnbrs_cnt = 0;
    auto label_cnt = data_graph->getLabelsCount();
    vector<ui> unbrs_labels, vnbrs_labels;
    unbrs_labels.reserve(label_cnt), vnbrs_labels.reserve(label_cnt);
    for (VertexID u = 0; u < qnum; u++) {
        auto ulabel = query_graph->getVertexLabel(u);
        auto vlabel_vs = data_graph->getVerticesByLabel(ulabel, vlabel_vs_cnt);
        auto unbrs = query_graph->getVertexNeighbors(u, unbrs_cnt);
        unbrs_labels.clear();
        for (ui i = 0; i < unbrs_cnt; i++) {
            auto& unbr = unbrs[i];
            auto unbr_label = query_graph->getVertexLabel(unbr);
            unbrs_labels.emplace_back(unbr_label);
        }
        sort(unbrs_labels.begin(), unbrs_labels.end());
        for (ui v_idx = 0; v_idx < vlabel_vs_cnt; v_idx++) {
            auto& v = vlabel_vs[v_idx];
            auto vnbrs = data_graph->getVertexNeighbors(v, vnbrs_cnt);
            vnbrs_labels.clear();
            for (ui i = 0; i < vnbrs_cnt; i++) {
                auto& vnbr = vnbrs[i];
                auto vnbr_label = data_graph->getVertexLabel(vnbr);
                vnbrs_labels.emplace_back(vnbr_label);
            }
            sort(vnbrs_labels.begin(), vnbrs_labels.end());
            if (SetOp::haveOverlapTwo(unbrs_labels, vnbrs_labels)) cans[u].emplace_back(v);
        }
    }

    for (ui i = 0; i < qnum; ++i) sort(cans[i].begin(), cans[i].end());
    aux.buildData(data_graph, query_graph, mem);
}

// --- 核心逻辑：确保候选点存在 ---
void CSMIndex::ensure_candidate_global(VertexID u, VertexID v_can) {
    auto& u_cans = aux.cans[u];

    ui idx = b_search::lower_bound_idx(u_cans, v_can);
    if (idx < u_cans.size() && u_cans[idx] == v_can) {
        return; // 已存在
    }
    u_cans.insert(u_cans.begin() + idx, v_can);
}

// --- 辅助函数：在已知行(row_idx)中插入一条边 v_nbr ---
void CSMIndex::insert_edge(CSMEdges* edges, VertexID v_src, VertexID v_nbr) {
    if (edges == nullptr) return;

    auto& row = edges->edge_map[v_src]; 
    ui idx = b_search::lower_bound_idx(row, v_nbr);
    if (idx == row.size() || row[idx] != v_nbr) {
        row.insert(row.begin() + idx, v_nbr); // 保持邻接表有序
    }
}

// --- 辅助函数：在已知行(row_idx)中删除一条边 v_nbr ---
void CSMIndex::delete_edge(CSMEdges* edges, VertexID v_src, VertexID v_nbr) {
    if (edges == nullptr) return;

    auto it = edges->edge_map.find(v_src);
    if (it == edges->edge_map.end()) return;

    auto& row = it->second;
    ui idx = b_search::lower_bound_idx(row, v_nbr);
    if (idx < row.size() && row[idx] == v_nbr) {
        row.erase(row.begin() + idx);
    }
}

void CSMIndex::update_Aux(Update de, vector<Edge>& matched_edges) {
    auto& dnum = aux.dnum;
    auto& all_edges = aux.data;

    auto v_src = de.edge_.src();
    auto v_dst = de.edge_.dst();
    char op = de.op_; // '+' or '-'

    for (auto& qe : matched_edges) {
        auto u_src = qe.src();
        auto u_dst = qe.dst();

        if (op == '+') {  // === 插入操作 ===
            if (v_src >= dnum) dnum = v_src + 1;
            if (v_dst >= dnum) dnum = v_dst + 1;

            // 1. 确保候选点存在 (会触发全局同步)
            ensure_candidate_global(u_src, v_src);
            ensure_candidate_global(u_dst, v_dst);
            // assert(aux.cans[u_src].size() == aux.data[u_src][u_dst]->edge_.size());
            // assert(aux.cans[u_dst].size() == aux.data[u_dst][u_src]->edge_.size());

            // 2. 插入边
            insert_edge(all_edges[u_src][u_dst], v_src, v_dst);
            insert_edge(all_edges[u_dst][u_src], v_dst, v_src);
        } else {  // === 删除操作 ===
            // 2. 删除边
            delete_edge(all_edges[u_src][u_dst], v_src, v_dst);
            delete_edge(all_edges[u_dst][u_src], v_dst, v_src);
        }
    }
}

// --- 辅助：种子点约束传播 (Seed Propagation) ---
bool CSMIndex::propagate_neighbor_constraint(const CSMIndex* global, ui u_fixed, VertexID v_fixed, uint64_t& in_queue) {
    auto& query_graph = aux.query_graph;
    auto& local_cans = this->aux.cans;
    auto& visited = mem->visited_bitmask[0];
    auto& m_heap = mem->m_heap;

    ui unbr_cnt = 0;
    const VertexID* unbrs = query_graph->getVertexNeighbors(u_fixed, unbr_cnt);

    for (ui i = 0; i < unbr_cnt; ++i) {
        ui unbr = unbrs[i];

        const vector<VertexID>& valid_candidates = global->aux.getNeighbors(u_fixed, unbr, v_fixed);
        if (valid_candidates.empty()) return false;

        if (!get_bit(visited, unbr)) {
            local_cans[unbr] = valid_candidates;
            set_bit(visited, unbr);
            set_bit(in_queue, unbr);
            m_heap.push(unbr, local_cans[unbr].size());
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
bool CSMIndex::propagate_forward(const CSMIndex* global, ui u, ui unbr) {
    auto& local_cans = this->aux.cans;
    auto& flag_array = mem->flag_array;
    auto& reset_buffer = mem->reset_buffer;
    auto& dnum = global->aux.dnum;
    auto& visited = mem->visited_bitmask[0];

    // 1. 清理 buffer
    reset_buffer.clear(); 

    // 2. Push & Deduplicate
    const auto& u_current_cans = local_cans[u];
    for (VertexID u_can : u_current_cans) {
        const vector<VertexID>& valid_neighbors = global->aux.getNeighbors(u, unbr, u_can);
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
        std::sort(reset_buffer.begin(), reset_buffer.end());
        for (VertexID v : reset_buffer) flag_array[v] = false;  // clear buffer before swap
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
bool CSMIndex::try_build_local(const CSMIndex* global, Edge de, Edge qe) {
    auto q_src = qe.src();
    auto q_dst = qe.dst();
    auto v_src = de.src();
    auto v_dst = de.dst();

    auto& qnum = aux.qnum;
    const auto& all_visited = mem->all_visited;
    auto& local_cans = this->aux.cans;
    auto& flag_array = mem->flag_array;
    auto& dnum = global->aux.dnum;
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
    if (!propagate_neighbor_constraint(global, q_src, v_src, in_queue)) return false;
    if (!propagate_neighbor_constraint(global, q_dst, v_dst, in_queue)) return false;

    // 传播剪枝
    while (!m_heap.empty()) {
        ui u = m_heap.pop();
        clear_bit(in_queue, u);

        if (local_cans[u].empty()) return false;

        ui unbr_cnt = 0;
        const VertexID* unbrs = aux.query_graph->getVertexNeighbors(u, unbr_cnt);

        for (ui i = 0; i < unbr_cnt; ++i) {
            ui unbr = unbrs[i];

            bool was_visited = get_bit(visited, unbr);
            ui old_size = was_visited ? local_cans[unbr].size() : 0;

            // 执行传播
            if (!propagate_forward(global, u, unbr)) return false;

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
    for (ui i = 0; i < qnum; ++i) {
        if (!get_bit(visited, i)) {
            local_cans[i] = global->aux.cans[i];
        }
        if (local_cans[i].empty()) return false;
    }

    return true;
}
