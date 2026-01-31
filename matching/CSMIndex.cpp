#include "CSMIndex.h"
#include "utils/setOp.h"
#include "utils/bSearch.h"
#include "BuildAux.h"
#include <set>
#include "assert.h"
using namespace std;

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
void Aux::buildData(const Graph* data_graph, const Graph* query_graph, CSMPruneCache* pruneCache) {
    dnum = data_graph->getVerticesCount();
    auto getNeighbors_ptr = [&](VertexID, VertexID, VertexID v, ui& count) {
        return data_graph->getVertexNeighbors(v, count);
    };
    BuildAux::buildAux(dnum, getNeighbors_ptr, query_graph, cans, data, pruneCache->flag_array, pruneCache->reset_buffer, pruneCache->visited_bitmask);
}
void Aux::updateData(const Aux& global, CSMPruneCache* pruneCache) {
    dnum = global.dnum;
    auto getNeighbors_vec = [&](VertexID u1, VertexID u2, VertexID v, ui& /*count*/) -> const vector<VertexID>& {
        return global.getNeighbors(u1, u2, v);
    };
    BuildAux::buildAux(dnum, getNeighbors_vec, query_graph, cans, data, pruneCache->flag_array, pruneCache->reset_buffer, pruneCache->visited_bitmask);
}

CSMPruneCache* CSMIndex::pruneCache = nullptr;
CSMPruneCache::CSMPruneCache(ui max_cans, ui qnum, ui dnum) {
    if (qnum > 64) {
        cout << "do not support query with #vertex > 64" << endl;
        exit(-1);
    }
    order = new VertexID[qnum];
    bfs_q.init(qnum);
    visited_bitmask = new uint64_t[64];
    all_visited = (qnum == 64) ? ~0ULL : ((1ULL << qnum) - 1);
    flag_array.resize(dnum, 0);
    reset_buffer.reserve(max_cans);
}

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
    pruneCache = new CSMPruneCache(max_cans, qnum, dnum);
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
    aux.buildData(data_graph, query_graph, pruneCache);
}

// --- 核心逻辑：确保候选点存在，若不存在则更新全局并同步所有邻居 ---
// 返回该候选点在 cans[u] 中的 index
ui CSMIndex::ensure_candidate_global(ui u, ui v_can) {
    auto& query_graph = aux.query_graph;
    auto& u_cans = aux.cans[u];

    // 1. 查找位置 & insert
    ui idx = b_search::lower_bound_idx(u_cans, v_can);
    if (idx < u_cans.size() && u_cans[idx] == v_can) {
        return idx;
    }
    u_cans.insert(u_cans.begin() + idx, v_can);

    // 2. 同步更新 u 的所有邻居的 edge_matrix
    // 即使不是当前正在处理的边，也需要插入一个空行，以保持 offset 索引与 cans 一致
    ui u_nbrs_cnt = 0;
    const VertexID* u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_cnt);

    for (ui i = 0; i < u_nbrs_cnt; ++i) {
        ui nbr = u_nbrs[i];
        CSMEdges* edges = aux.data[u][nbr];
        if (edges == nullptr) {
            cerr << "insert v_edge to un-existed u_edge(" << u << "->" << nbr << ")" << endl;
            exit(-1);
        }
        edges->edge_.insert(edges->edge_.begin() + idx, vector<VertexID>());
        assert(edges->edge_.size() == aux.cans[u].size());
    }

    return idx;
}

// 仅查找 index
ui CSMIndex::find_candidate_index(ui u, ui v_can) {
    auto& u_cans = aux.cans[u];
    if (u_cans.empty()) return -1;

    ui idx = b_search::lower_bound_idx(u_cans, v_can);
    if (idx < u_cans.size() && u_cans[idx] == v_can) {
        return idx;
    }
    return -1;
}

// --- 辅助函数：在已知行(row_idx)中插入一条边 v_nbr ---
void CSMIndex::insert_edge_at_index(CSMEdges* edges, ui row_idx, ui v_nbr) {
    if (edges == nullptr) return;

    auto& row = edges->edge_[row_idx];
    ui idx = b_search::lower_bound_idx(row, v_nbr);
    if (idx == row.size() || row[idx] != v_nbr) {
        row.insert(row.begin() + idx, v_nbr);
    }
}

// --- 辅助函数：在已知行(row_idx)中删除一条边 v_nbr ---
void CSMIndex::delete_edge_at_index(CSMEdges* edges, ui row_idx, ui v_nbr) {
    if (edges == nullptr) return;

    auto& row = edges->edge_[row_idx];
    ui idx = b_search::lower_bound_idx(row, v_nbr);
    if (idx < row.size() && row[idx] == v_nbr) {
        row.erase(row.begin() + idx);
    }
}

void CSMIndex::update_Aux(Update de, vector<Edge>& matched_edges) {
    auto& dnum = aux.dnum;
    auto& all_edges = aux.data;
    // 记录处理过的点对 {min, max} 防止无向图重复处理
    set<pair<ui, ui>> processed_pairs;
    
    auto v_src = de.edge_.src();
    auto v_dst = de.edge_.dst();
    char op = de.op_; // '+' or '-'

    for (auto& qe : matched_edges) {
        auto u_src = qe.src();
        auto u_dst = qe.dst();

        pair<ui, ui> key = minmax(u_src, u_dst);
        if (processed_pairs.count(key)) continue;
        processed_pairs.insert(key);

        if (op == '+') {  // === 插入操作 ===
            if (v_src >= dnum) dnum = v_src + 1;
            if (v_dst >= dnum) dnum = v_dst + 1;

            // 1. 确保候选点存在 (会触发全局同步)
            ui src_idx = ensure_candidate_global(u_src, v_src);
            ui dst_idx = ensure_candidate_global(u_dst, v_dst);
            // assert(aux.cans[u_src].size() == aux.data[u_src][u_dst]->edge_.size());
            // assert(aux.cans[u_dst].size() == aux.data[u_dst][u_src]->edge_.size());

            // 2. 插入边
            insert_edge_at_index(all_edges[u_src][u_dst], src_idx, v_dst);
            insert_edge_at_index(all_edges[u_dst][u_src], dst_idx, v_src);
        } else if (op == '-') {  // === 删除操作 ===
            // 1. 查找候选点位置
            ui src_idx = find_candidate_index(u_src, v_src);
            ui dst_idx = find_candidate_index(u_dst, v_dst);
            if (src_idx == (ui)-1 || dst_idx == (ui)-1) continue;

            // 2. 删除边
            delete_edge_at_index(all_edges[u_src][u_dst], src_idx, v_dst);
            delete_edge_at_index(all_edges[u_dst][u_src], dst_idx, v_src);
        }
    }
}

// --- 辅助：种子点约束传播 (Seed Propagation) ---
// 将确定匹配点 (u_fixed -> v_fixed) 的约束传播给 u_fixed 的所有邻居
bool CSMIndex::propagate_neighbor_constraint(const CSMIndex* global, ui u_fixed, VertexID v_fixed) {
    auto& query_graph = aux.query_graph;
    auto& local_cans = this->aux.cans;
    auto& visited = pruneCache->visited_bitmask[0];
    auto& bfs_q = pruneCache->bfs_q;

    ui unbr_cnt = 0;
    const VertexID* unbrs = query_graph->getVertexNeighbors(u_fixed, unbr_cnt);

    for (ui i = 0; i < unbr_cnt; ++i) {
        ui unbr = unbrs[i];

        // 获取约束下的合法候选集
        const vector<VertexID>& valid_candidates = global->aux.getNeighbors(u_fixed, unbr, v_fixed);
        if (valid_candidates.empty()) return false;

        bool updated = false;
        if (!get_bit(visited, unbr)) {  // 第一次访问该邻居, 直接赋值
            local_cans[unbr] = valid_candidates; 
            set_bit(visited, unbr);
            bfs_q.push(unbr);
        } else {  // 已经访问过, 求交
            SetOp::intersectAndUpdate(local_cans[unbr], valid_candidates);
            if (local_cans[unbr].empty()) return false;
        }
    }
    return true;
}

// --- 快速剪枝 ---
bool CSMIndex::edge_quick_prune(const CSMIndex* global, Edge de, Edge qe) {
    if (!propagate_neighbor_constraint(global, qe.src(), de.src())) return false;
    if (!propagate_neighbor_constraint(global, qe.dst(), de.dst())) return false;
    return true;
}

// --- 通用前向传播 (BFS Step) ---
bool CSMIndex::propagate_forward(const CSMIndex* global, ui u, ui unbr) {
    auto& local_cans = this->aux.cans;
    auto& flag_array = pruneCache->flag_array;
    auto& reset_buffer = pruneCache->reset_buffer;
    auto& dnum = global->aux.dnum;
    auto& visited = pruneCache->visited_bitmask[0];

    // 1. 清理 buffer
    reset_buffer.clear(); 

    // 2. Push & Deduplicate
    const auto& u_current_cans = local_cans[u];
    for (VertexID u_can : u_current_cans) {
        const vector<VertexID>& valid_neighbors = global->aux.getNeighbors(u, unbr, u_can);
        for (VertexID v_nbr : valid_neighbors) {
            if (flag_array[v_nbr] == 0) {  // 去重
                flag_array[v_nbr] = 1;
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
        for (VertexID v : reset_buffer) flag_array[v] = 0;  // clear buffer before swap
        local_cans[unbr].swap(reset_buffer);
    } else {  // unbr 已访问, 过滤现有的 local_cans[unbr]，只保留 flag 为 1 的元素
        auto& target = local_cans[unbr];
        ui write_idx = 0;
        for (ui read_idx = 0; read_idx < target.size(); ++read_idx) {
            VertexID v = target[read_idx];
            if (flag_array[v] == 1) {
                target[write_idx++] = v;
            }
        }
        target.resize(write_idx);

        for (auto& v : reset_buffer) flag_array[v] = 0;  // clear buffer

        if (target.empty()) return false;
    }

    return true;
}

// --- 构建局部索引主流程 ---
bool CSMIndex::try_build_local(const CSMIndex* global, Edge de, Edge qe) {
    auto& qnum = aux.qnum;
    const auto& all_visited = pruneCache->all_visited;
    auto& local_cans = this->aux.cans;
    auto& flag_array = pruneCache->flag_array;
    auto& dnum = global->aux.dnum;
    auto& visited = pruneCache->visited_bitmask[0];
    visited = 0;
    auto& bfs_q = pruneCache->bfs_q;
    bfs_q.clear();

    auto q_src = qe.src();
    auto q_dst = qe.dst();
    local_cans[q_src].clear();
    local_cans[q_src].emplace_back(de.src());
    set_bit(visited, q_src);
    local_cans[q_dst].clear();
    local_cans[q_dst].emplace_back(de.dst());
    set_bit(visited, q_dst);

    // 快速剪枝
    if (!edge_quick_prune(global, de, qe)) return false;

    // update when necessary
    if (flag_array.size() < dnum) flag_array.resize(dnum, 0);

    // 传播剪枝
    while (!bfs_q.empty()) {
        ui u = bfs_q.front();
        bfs_q.pop();

        if (local_cans[u].empty()) return false;

        ui unbr_cnt = 0;
        const VertexID* unbrs = aux.query_graph->getVertexNeighbors(u, unbr_cnt);

        for (ui i = 0; i < unbr_cnt; ++i) {
            ui unbr = unbrs[i];

            // 记录状态
            bool was_visited = get_bit(visited, unbr);
            // 自适应感知优化
            // if (was_visited && local_cans[unbr].size() < 3) continue;
            // 执行传播
            if (!propagate_forward(global, u, unbr)) return false;

            if (!was_visited) {
                set_bit(visited, unbr);
                bfs_q.push(unbr);
            }
        }
    }

    // 5. 孤立点检查 (处理非连通图)
    if (LIKELY(visited == all_visited)) return true;
    for (ui i = 0; i < qnum; ++i) {
        if (!get_bit(visited, i)) {
            local_cans[i] = global->aux.cans[i];
        }
        if (local_cans[i].empty()) return false;
    }

    return true;
}
