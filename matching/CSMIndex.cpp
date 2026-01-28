#include "CSMIndex.h"
#include "utils/SetOp.h"
#include "utils/bSearch.h"
#include "BuildAux.h"
#include<set>
using namespace std;

const vector<VertexID>& Aux::getNeighbors(VertexID u_1, VertexID u_2, VertexID v) const {
    auto v_idx = b_search::lower_bound_idx(cans[u_1], v);
    if (v_idx == cans[u_1].size()  || cans[u_1][v_idx] != v) {
        return empty;
    }
    auto& edges = *(data[u_1][u_2]);
    return edges[v_idx];
}
void Aux::init(const Graph* data_graph, const Graph* query_graph) {
    this->query_graph = query_graph;
    dnum = data_graph->getVerticesCount();
    qnum = query_graph->getVerticesCount();

    data = new CSMEdges **[qnum];
    for (ui i = 0; i < qnum; ++i) {
        data[i] = new CSMEdges *[qnum];
        for (ui j = 0; j < qnum; ++j) {
            data[i][j] = nullptr;
        }
    }
    auto getNeighbors_ptr = [&](VertexID, VertexID, VertexID v, ui& count) {
        return data_graph->getVertexNeighbors(v, count);
    };
    BuildAux::buildAux(dnum, getNeighbors_ptr, query_graph, cans, data);
}
void Aux::init(const Graph* query_graph) {
    qnum = query_graph->getVerticesCount();
    this->query_graph = query_graph;
    cans.resize(qnum);
    data = new CSMEdges **[qnum];
    for (ui i = 0; i < qnum; ++i) {
        data[i] = new CSMEdges *[qnum];
        for (ui j = 0; j < qnum; ++j) {
            data[i][j] = nullptr;
        }
    }
}
void Aux::update(const Aux& global) {
    this->dnum = global.dnum;
    for (ui i = 0; i < qnum; i++) {
        auto u = i;
        ui u_nbrs_count;
        auto u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_count);
        for (ui k = 0; k < u_nbrs_count; ++k) { 
            VertexID u_nbr = u_nbrs[k];
            // 只删除 u 出发的方向,当循环到 u_nbr 时，它会负责删除 edge_matrix[u_nbr][u]
            if (data[u][u_nbr] != nullptr) {
                delete data[u][u_nbr];
                data[u][u_nbr] = nullptr;
            }
        }
    }
    auto getNeighbors_vec = [&](VertexID u1, VertexID u2, VertexID v, ui& /*count*/) -> const vector<VertexID>& {
        return global.getNeighbors(u1, u2, v);
    };
    BuildAux::buildAux(dnum, getNeighbors_vec, query_graph, cans, data);
}

CSMPruneCache* CSMIndex::pruneCache = nullptr;
CSMPruneCache::CSMPruneCache(ui max_cans, ui qnum) {
    order = new VertexID[qnum];
    visited.resize(qnum);
    u_cans_nbrs.reserve(max_cans);
    results_buffer.reserve(max_cans);
    aux_cursors.reserve(max_cans);
    aux_queue.reserve(max_cans);
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
    cans.resize(qnum);
    auto max_cans = data_graph->getGraphMaxLabelFrequency();

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
    aux.init(data_graph, query_graph);

    pruneCache = new CSMPruneCache(max_cans, qnum);
}

// --- 核心逻辑：确保候选点存在，若不存在则更新全局并同步所有邻居 ---
// 返回该候选点在 cans[u] 中的 index
ui CSMIndex::ensure_candidate_global(ui u, ui v_can) {
    auto& query_graph = aux.query_graph;
    auto& u_cans = aux.cans[u];

    // 1. 查找位置 & insert
    auto it = std::lower_bound(u_cans.begin(), u_cans.end(), v_can);
    ui idx = std::distance(u_cans.begin(), it);
    if (it != u_cans.end() && *it == v_can) {
        return idx;
    }
    u_cans.insert(it, v_can);

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
    }

    return idx;
}

// 仅查找 index
ui CSMIndex::find_candidate_index(ui u, ui v_can) {
    auto& u_cans = aux.cans[u];
    if (u_cans.empty()) return -1;

    auto it = std::lower_bound(u_cans.begin(), u_cans.end(), v_can);
    if (it != u_cans.end() && *it == v_can) {
        return std::distance(u_cans.begin(), it);
    }
    return -1;
}

// --- 辅助函数：在已知行(row_idx)中插入一条边 v_nbr ---
void CSMIndex::insert_edge_at_index(CSMEdges* edges, ui row_idx, ui v_nbr) {
    if (edges == nullptr) return;

    auto& row = edges->edge_[row_idx];
    auto it = std::lower_bound(row.begin(), row.end(), v_nbr);

    if (it == row.end() || *it != v_nbr) {
        row.insert(it, v_nbr);
    }
}

// --- 辅助函数：在已知行(row_idx)中删除一条边 v_nbr ---
void CSMIndex::delete_edge_at_index(CSMEdges* edges, ui row_idx, ui v_nbr) {
    if (edges == nullptr) return;

    auto& row = edges->edge_[row_idx];
    auto it = std::lower_bound(row.begin(), row.end(), v_nbr);

    if (it != row.end() && *it == v_nbr) {
        row.erase(it);
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

bool CSMIndex::try_build_local(const CSMIndex* global, Edge de, Edge qe) {
    auto& qnum = aux.qnum;
    bool valid = true;
    auto& order = pruneCache->order;
    auto& global_cans = global->aux.cans;
    auto& local_cans = this->aux.cans;
    auto& visited = pruneCache->visited;

    auto d_src = de.src();
    auto d_dst = de.dst();
    auto q_src = qe.src();
    auto q_dst = qe.dst();

    // 1. 初始化容器
    local_cans.resize(qnum);

    // 2. 重置 visited 标记
    visited.resize(qnum);
    std::fill(visited.begin(), visited.end(), false);

    // 3. 设置种子点 (Seeds) 并标记为已访问
    local_cans[q_src].clear();
    local_cans[q_src].push_back(d_src);
    visited[q_src] = true;
    local_cans[q_dst].clear();
    local_cans[q_dst].push_back(d_dst);
    visited[q_dst] = true;

    // --- 3. 1-Hop Neighbor Quick Prune ---
    if (!edge_quick_prune(global, de, qe)) return false;

    // 4. 构建处理顺序 (启发式：Candidates 少的优先)
    for (ui i = 0; i < qnum; i++) order[i] = i;

    sort(order, order+qnum, [&](ui id_a, ui id_b) {
        size_t size_a = visited[id_a] ? local_cans[id_a].size() : global_cans[id_a].size();
        size_t size_b = visited[id_b] ? local_cans[id_b].size() : global_cans[id_b].size();
        return size_a < size_b;
    });

    // Pass 1: Forward Prune
    for (ui idx = 0; idx < qnum && valid; idx++) {
        auto u = order[idx];
        valid = csm_prune(u, global);
    }
    // Pass 2: Backward Prune
    ui idx = qnum;
    while (idx > 0 && valid) {
        idx--;
        auto u = order[idx];
        valid = csm_prune(u, global);
    }

    if (!valid) return false;

    // 5. 构建 Local Edge Matrix
    this->aux.update(global->aux); 

    return true;
}

// 辅助：将确定匹配点 (u_fixed -> v_fixed) 的约束传播给 u_fixed 的邻居
bool CSMIndex::propagate_neighbor_constraint(const CSMIndex* global, ui u_fixed, VertexID v_fixed) {
    auto& query_graph = aux.query_graph;
    auto& local_cans = this->aux.cans;
    auto& visited = pruneCache->visited;

    ui unbr_cnt = 0;
    const VertexID* unbrs = query_graph->getVertexNeighbors(u_fixed, unbr_cnt);

    for (ui i = 0; i < unbr_cnt; ++i) {
        ui unbr = unbrs[i];

        // 调用 Global Index 直接获取合法的候选点列表
        const vector<VertexID>& valid_candidates = global->aux.getNeighbors(u_fixed, unbr, v_fixed);
        if (valid_candidates.empty()) return false;

        if (!visited[unbr]) {
            // Case A: 第一次访问该邻居 -> 直接赋值 (Copy)
            local_cans[unbr] = valid_candidates;
            visited[unbr] = true;
        } else {
            // Case B: 已经访问过 (例如是三角形的另一个顶点，或者已被另一个端点约束过) -> 求交集
            SetOp::intersectAndUpdate(local_cans[unbr], valid_candidates);
            if (local_cans[unbr].empty()) return false;
        }
    }
    return true;
}

bool CSMIndex::edge_quick_prune(const CSMIndex* global, Edge de, Edge qe) {
    if (!propagate_neighbor_constraint(global, qe.src(), de.src())) return false;
    if (!propagate_neighbor_constraint(global, qe.dst(), de.dst())) return false;
    return true;
}

bool CSMIndex::csm_prune(ui u, const CSMIndex* global) {
    // 获取 Cache 资源
    auto& u_cans_nbrs = pruneCache->u_cans_nbrs;          // pointer array
    auto& results_buffer = pruneCache->results_buffer;
    auto& aux_cursors = pruneCache->aux_cursors;
    auto& aux_queue = pruneCache->aux_queue;
    auto& visited = pruneCache->visited;

    auto& query_graph = aux.query_graph; 
    auto& local_cans = this->aux.cans;
    const auto& global_cans = global->aux.cans;

    // --- A. Lazy Initialization ---
    if (!visited[u]) {
        local_cans[u] = global_cans[u]; // vector deep copy
        visited[u] = true;
    }
    if (local_cans[u].empty()) {
        return false;
    }

    ui unbr_cnt = 0;
    const VertexID* unbrs = query_graph->getVertexNeighbors(u, unbr_cnt);
    ui valid_cans_cnt = local_cans[u].size(); // 当前有效的 candidates 数量

    // --- B. 遍历查询图邻居 ---
    for (ui unbr_idx = 0; unbr_idx < unbr_cnt; unbr_idx++) {
        VertexID unbr = unbrs[unbr_idx];

        // --- C. Hybrid View, 获取邻居的 Candidates ---
        const vector<VertexID>& unbr_candidates = visited[unbr] ? local_cans[unbr] : global_cans[unbr];

        // 如果邻居连 Candidates 都没有，说明拓扑无法匹配
        if (unbr_candidates.empty()) {
            local_cans[u].clear();
            return false;
        }

        // --- D. 准备 SetOp 数据 (获取数据图邻接关系) ---
        for (ui can_idx = 0; can_idx < valid_cans_cnt; can_idx++) {
            VertexID u_can = local_cans[u][can_idx];
            const vector<VertexID>& nbrs_vec = global->aux.getNeighbors(u, unbr, u_can);
            u_cans_nbrs[can_idx] = &nbrs_vec;
        }

        // --- E. 集合求交 (Semi-join) ---
        // 验证 u 的每个 candidate 的邻居，是否出现在 unbr_candidates 中
        SetOp::multi_overlap_no_alloc(u_cans_nbrs, valid_cans_cnt, unbr_candidates,
                                      results_buffer, aux_cursors, aux_queue);

        // --- F. 压缩 Local Candidates (原地移除无效点) ---
        ui new_valid = 0;
        for (ui valid_can_idx = 0; valid_can_idx < valid_cans_cnt; valid_can_idx++) {
            if (results_buffer[valid_can_idx] == true) {
                // 保留通过检查的点
                // 避免自赋值：仅当 new_valid != valid_can_idx 时才赋值
                if (new_valid != valid_can_idx) {
                    local_cans[u][new_valid] = local_cans[u][valid_can_idx];
                }
                new_valid++;
            }
        }
        
        // 必须 resize，真正释放尾部空间
        local_cans[u].resize(new_valid);
        valid_cans_cnt = new_valid; // 更新当前计数

        // 提前退出：如果当前点已经剪空
        if (valid_cans_cnt == 0) return false;
    }

    return valid_cans_cnt > 0;
}

inline void CSMIndex::copy_block(ui* dst, const ui* src, ui len) {
    if (len > 0 && src != nullptr && dst != nullptr) {
        copy(src, src + len, dst);
    }
}