#include "CSMIndex.h"
#include "utils/SetOp.h"
#include "utils/bSearch.h"
#include "BuildEdgeIndex.h"
#include<set>
using namespace std;

CSMPruneCache* CSMIndex::pruneCache = nullptr;
void CSMPruneCache::update(ui new_max_cans, ui new_qnum) {
    if (new_qnum <= this->qnum && new_max_cans <= this->max_cans) {
        return; 
    }

    // delete
    if (u_cans_nbrs != nullptr) {
        delete[] order;
        delete[] u_cans_nbrs;
        delete[] u_cans_nbrs_cnt;
        delete[] results_buffer;
        delete[] aux_cursors;
        delete[] aux_queue;
    }

    // renew
    this->qnum = new_qnum;
    this->max_cans = new_max_cans;
    order = new VertexID[qnum]; 
    u_cans_nbrs = new const ui*[max_cans];
    u_cans_nbrs_cnt = new ui[max_cans];
    results_buffer = new bool[max_cans];
    aux_cursors = new ui[max_cans];
    aux_queue = new ui[max_cans];
}
CSMPruneCache::~CSMPruneCache () {
    if (u_cans_nbrs != nullptr) {
        delete[] u_cans_nbrs;
        delete[] u_cans_nbrs_cnt;
        delete[] order;
        delete[] results_buffer;
        delete[] aux_cursors;
        delete[] aux_queue;
    }
}

// 只选择符合要求的边，然后构建辅助数据结构。以边为主要过滤条件，且不做传播剪枝
void CSMIndex::build_A(const Graph *data_graph, const Graph *query_graph) {
// 首先构建每个点的candidates，然后构建边
// 之后可以做优化，直接识别所有查询边可以匹配的数据边，然后获取每个查询点的带有重复的 cans, 然后对每个点的candidates做重排序。
// 或者可以直接按照边来做查询。
    this->query_graph = query_graph;
    dnum = data_graph->getVerticesCount();
    d_edge_num = data_graph->getEdgesCount();
    auto qnum = query_graph->getVerticesCount();
    max_cans = data_graph->getGraphMaxLabelFrequency();
    cans_cnt = new ui[qnum];
    memset(cans_cnt, 0, sizeof(ui) * qnum);
    cans = new ui*[qnum];
    for (ui i = 0; i < qnum; ++i) {
        cans[i] = new ui[max_cans];
    }

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
            if (SetOp::haveOverlapTwo(unbrs_labels, vnbrs_labels)) cans[u][cans_cnt[u]++] = v;
        }
    }

    for (ui i = 0; i < qnum; ++i) {
        sort(cans[i], cans[i] + cans_cnt[i]);
    }

    edge_matrix = new Edges **[qnum];
    for (ui i = 0; i < qnum; ++i) {
        edge_matrix[i] = new Edges *[qnum];
    }
    BuildEdgeIndex::buildCansIndex(data_graph, query_graph, cans, cans_cnt, edge_matrix);

    pruneCache = new CSMPruneCache(max_cans, qnum);
}

// --- 辅助函数：仅在 offset 中插入一个空行（用于同步其他邻居的索引） ---
// old_edges: 旧的边结构
// insert_idx: 在 cans 中的插入位置（即第几行）
Edges* CSMIndex::insert_blank_row(Edges* old_edges, ui insert_idx) {
    Edges* new_edges = new Edges();
    ui old_v_cnt = old_edges->vertex_count_;

    new_edges->vertex_count_ = old_v_cnt + 1;
    new_edges->edge_count_ = old_edges->edge_count_; // 边数不变
    new_edges->max_degree_ = old_edges->max_degree_;

    new_edges->offset_ = new ui[new_edges->vertex_count_ + 1];
    new_edges->edge_ = new ui[new_edges->edge_count_];

    // 1. 复制 offset (前半部分)
    copy_block(new_edges->offset_, old_edges->offset_, insert_idx + 1);

    // 2. 插入空行 (新行的 offset 等于上一行的结束位置，即不增加边)
    // insert_idx 是新插入点的位置，insert_idx + 1 对应新点的下一个 offset
    new_edges->offset_[insert_idx + 1] = old_edges->offset_[insert_idx];

    // 3. 复制并调整 offset (后半部分，因为没加边，值其实和旧的一样，但位置后移了一位)
    for (ui i = insert_idx + 1; i <= old_v_cnt; ++i) {
        new_edges->offset_[i + 1] = old_edges->offset_[i]; 
    }

    // 4. 复制 edge (完全直通，因为没有加新边)
    copy_block(new_edges->edge_, old_edges->edge_, old_edges->edge_count_);

    return new_edges;
}

// --- 辅助函数：在已知行(row_idx)中插入一条边 v_nbr ---
// 对应原代码的情况 B，但修复了越界问题
Edges* CSMIndex::insert_edge_at_index(Edges* old_edges, ui row_idx, ui v_nbr) {
    ui start = old_edges->offset_[row_idx];
    ui end = old_edges->offset_[row_idx + 1];
    ui len = end - start;

    // 查找插入位置
    ui nbr_insert_pos = b_search::lower_bound_idx(old_edges->edge_ + start, len, v_nbr);

    // 查重
    if (nbr_insert_pos < len && old_edges->edge_[start + nbr_insert_pos] == v_nbr) {
        return nullptr; // 重复边，不做操作
    }

    Edges* new_edges = new Edges();
    new_edges->vertex_count_ = old_edges->vertex_count_;
    new_edges->edge_count_ = old_edges->edge_count_ + 1;

    // 更新 max_degree
    new_edges->max_degree_ = max(old_edges->max_degree_, len + 1);

    new_edges->offset_ = new ui[new_edges->vertex_count_ + 1];
    new_edges->edge_ = new ui[new_edges->edge_count_];

    // 1. 复制并更新 Offset
    copy_block(new_edges->offset_, old_edges->offset_, row_idx + 1);
    for (ui i = row_idx + 1; i <= new_edges->vertex_count_; ++i) {
        new_edges->offset_[i] = old_edges->offset_[i] + 1; // 后续所有 offset +1
    }

    // 2. 复制 Edge 并插入新值
    // Part 1: start 之前
    copy_block(new_edges->edge_, old_edges->edge_, start);
    
    // Part 2: 当前行内
    ui global_insert_pos = start + nbr_insert_pos;
    copy_block(new_edges->edge_ + start, old_edges->edge_ + start, nbr_insert_pos);
    new_edges->edge_[global_insert_pos] = v_nbr;
    copy_block(new_edges->edge_ + global_insert_pos + 1, 
               old_edges->edge_ + global_insert_pos, 
               len - nbr_insert_pos);

    // Part 3: end 之后
    copy_block(new_edges->edge_ + end + 1, 
               old_edges->edge_ + end, 
               old_edges->edge_count_ - end);

    return new_edges;
}

// --- 核心逻辑：确保候选点存在，若不存在则更新全局并同步所有邻居 ---
// 返回该候选点在 cans[u] 中的 index
ui CSMIndex::ensure_candidate_global(ui u, ui v_can) {
    ui* current_cans = cans[u];
    ui current_cnt = cans_cnt[u];

    // 1. 查找位置
    ui idx = b_search::lower_bound_idx(current_cans, current_cnt, v_can);

    // 如果已经存在，直接返回索引
    if (idx < current_cnt && current_cans[idx] == v_can) {
        return idx;
    }

    // 2. 需要插入新候选点
    // A. 更新 cans 数组
    ui new_count = current_cnt + 1;
    ui* new_cans = new ui[new_count];
    if (max_cans < new_count) max_cans = new_count;

    copy_block(new_cans, current_cans, idx);
    new_cans[idx] = v_can;
    copy_block(new_cans + idx + 1, current_cans + idx, current_cnt - idx);

    delete[] cans[u];
    cans[u] = new_cans;
    cans_cnt[u] = new_count;

    // B. 同步更新 u 的所有邻居的 edge_matrix
    // 即使不是当前正在处理的边，也需要插入一个空行，以保持 offset 索引与 cans 一致
    ui u_nbrs_cnt = 0;
    const VertexID* u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_cnt);

    for (ui i = 0; i < u_nbrs_cnt; ++i) {
        ui nbr = u_nbrs[i];
        Edges* old_edges = edge_matrix[u][nbr];
        
        // 必须确保 old_edges 非空 (一般 construct 阶段已初始化)
        if (old_edges == nullptr) {
            cout << "Error: Edge matrix not initialized for " << u << "-" << nbr << endl;
            exit(-1);
        }

        Edges* new_edges = insert_blank_row(old_edges, idx);
        
        delete old_edges;
        edge_matrix[u][nbr] = new_edges;
    }

    return idx;
}

// --- 辅助函数：在已知行(row_idx)中删除一条边 v_nbr ---
Edges* CSMIndex::delete_edge_at_index(Edges* old_edges, ui row_idx, ui v_nbr) {
    ui start = old_edges->offset_[row_idx];
    ui end = old_edges->offset_[row_idx + 1];
    ui len = end - start;

    // 1. 查找要删除的边是否存在
    ui nbr_idx_local = b_search::lower_bound_idx(old_edges->edge_ + start, len, v_nbr);
    
    // 如果没找到，或者找到的位置的值不是 v_nbr，说明边不存在，直接返回 nullptr
    if (nbr_idx_local == len || old_edges->edge_[start + nbr_idx_local] != v_nbr) {
        return nullptr; 
    }

    // 2. 构建新结构
    Edges* new_edges = new Edges();
    new_edges->vertex_count_ = old_edges->vertex_count_;
    // 边数减 1
    new_edges->edge_count_ = old_edges->edge_count_ - 1; 
    // max_degree 不变或减小，简单起见保持不变，或者遍历重新计算（这里选择保持不变以节省时间）
    new_edges->max_degree_ = old_edges->max_degree_;

    new_edges->offset_ = new ui[new_edges->vertex_count_ + 1];
    new_edges->edge_ = new ui[new_edges->edge_count_];

    // 3. 处理 Offset
    // row_idx 及其之前的 offset 不变
    copy_block(new_edges->offset_, old_edges->offset_, row_idx + 1);
    
    // row_idx 之后的所有 offset 都要减 1 (因为删除了1条边)
    for (ui i = row_idx + 1; i <= new_edges->vertex_count_; ++i) {
        new_edges->offset_[i] = old_edges->offset_[i] - 1;
    }

    // 4. 处理 Edge (跳过被删除的那个值)
    ui global_delete_pos = start + nbr_idx_local;

    // Part A: 删除位置之前的边
    copy_block(new_edges->edge_, old_edges->edge_, global_delete_pos);
    
    // Part B: 删除位置之后的边
    copy_block(new_edges->edge_ + global_delete_pos, 
               old_edges->edge_ + global_delete_pos + 1, 
               old_edges->edge_count_ - global_delete_pos - 1);

    return new_edges;
}

// 仅查找，如果没找到返回 -1 (或者 vertex_count)
// 用于删除操作，如果候选点都不在 cans 里，就没必要删边了
ui CSMIndex::find_candidate_index(ui u, ui v_can) {
    if (cans_cnt[u] == 0) return -1;

    ui idx = b_search::lower_bound_idx(cans[u], cans_cnt[u], v_can);
    if (idx < cans_cnt[u] && cans[u][idx] == v_can) {
        return idx;
    }
    return -1;
}

void CSMIndex::update_A(Update de, vector<Edge>& matched_edges) {
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

        if (op == '+') {
            // === 插入操作 ===
            d_edge_num++;
            if (v_src >= dnum) dnum = v_src + 1;
            if (v_dst >= dnum) dnum = v_dst + 1;
            
            // 1. 确保候选点存在 (会触发全局同步)
            ui src_idx = ensure_candidate_global(u_src, v_src);
            ui dst_idx = ensure_candidate_global(u_dst, v_dst);

            // 2. 插入边
            Edges* old_edges1 = edge_matrix[u_src][u_dst];
            Edges* new_edges1 = insert_edge_at_index(old_edges1, src_idx, v_dst);

            Edges* old_edges2 = edge_matrix[u_dst][u_src];
            Edges* new_edges2 = insert_edge_at_index(old_edges2, dst_idx, v_src);

            if (new_edges1) {
                delete edge_matrix[u_src][u_dst];
                edge_matrix[u_src][u_dst] = new_edges1;
            }
            if (new_edges2) {
                delete edge_matrix[u_dst][u_src];
                edge_matrix[u_dst][u_src] = new_edges2;
            }

        } else if (op == '-') {
            // === 删除操作 ===
            if (d_edge_num > 0) d_edge_num--;

            // 1. 查找候选点位置 (只读，不修改 cans)
            ui src_idx = find_candidate_index(u_src, v_src);
            ui dst_idx = find_candidate_index(u_dst, v_dst);

            // 如果连候选点都没找到，说明边肯定不存在，跳过
            if (src_idx == (ui)-1 || dst_idx == (ui)-1) continue;

            // 2. 删除边
            // 正向: 在 u_src 的 cans[src_idx] 行中删除 v_dst
            Edges* old_edges1 = edge_matrix[u_src][u_dst];
            Edges* new_edges1 = delete_edge_at_index(old_edges1, src_idx, v_dst);

            // 反向: 在 u_dst 的 cans[dst_idx] 行中删除 v_src
            Edges* old_edges2 = edge_matrix[u_dst][u_src];
            Edges* new_edges2 = delete_edge_at_index(old_edges2, dst_idx, v_src);

            if (new_edges1) {
                delete edge_matrix[u_src][u_dst];
                edge_matrix[u_src][u_dst] = new_edges1;
            }
            if (new_edges2) {
                delete edge_matrix[u_dst][u_src];
                edge_matrix[u_dst][u_src] = new_edges2;
            }
        }
    }

    // 更新 Cache 统计信息 (仅在扩容时必要，删除操作通常不需要更新 max_cans)
    if (op == '+' && max_cans > pruneCache->max_cans) {
        pruneCache->update(max_cans, query_graph->getVerticesCount());
    }
}

// 用于 local index 的构建
// filter: u 的 can 需要在其 unbrs 的 cans 中有 vnbrs
// 从 cans 最小开始，然后从其邻居选择最小的，然后反向做一次
bool CSMIndex::construct_local(CSMIndex* global) {
    this->query_graph = global->query_graph;
    auto qnum = query_graph->getVerticesCount();
    bool valid = true;
    auto& order = pruneCache->order;

    // build a simple order. Shortest First
    for (ui i = 0; i < qnum; i++) order[i] = i;
    sort(order, order + qnum, [&](ui id_a, ui id_b) {
        return cans_cnt[id_a] < cans_cnt[id_b];
    });

    // Pass 1: Forward Prune
    for (ui idx = 0; idx < qnum && valid; idx++) {
        auto& u = order[idx];
        valid = csmPrune(u, global);
    }
    // Pass 2: Backward Prune
    ui idx = qnum;
    while (idx > 0 && valid) {
        idx--;
        auto u = order[idx];
        valid = csmPrune(u, global);
    }
    if (!valid) return false;

    // re-construct edges
    if (edge_matrix == nullptr) {
        edge_matrix = new Edges **[qnum];
        for (ui i = 0; i < qnum; ++i) {
            edge_matrix[i] = new Edges *[qnum];
        }
    } else {
        for (ui i = 0; i < qnum; i++) {
            auto u = i;
            ui u_nbrs_count;
            const VertexID* u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_count);
            for (ui k = 0; k < u_nbrs_count; ++k) { 
                VertexID u_nbr = u_nbrs[k];
                // 只删除 u 出发的方向,当循环到 u_nbr 时，它会负责删除 edge_matrix[u_nbr][u]
                if (edge_matrix[u][u_nbr] != nullptr) {
                    delete edge_matrix[u][u_nbr];
                    edge_matrix[u][u_nbr] = nullptr;
                }
            }
        }
    }
    // TODO: an optimize: if cans of u & unbr do not change, do not re-construct edges
    buildCSMEdgeMatrix(global);
    return true;
}

bool CSMIndex::csmPrune(ui u, CSMIndex* global) {
    auto& u_cans_nbrs = pruneCache->u_cans_nbrs;
    auto& u_cans_nbrs_cnt = pruneCache->u_cans_nbrs_cnt;
    auto& results_buffer = pruneCache->results_buffer;
    auto& aux_cursors = pruneCache->aux_cursors;
    auto& aux_queue = pruneCache->aux_queue;

    ui unbr_cnt = 0;
    auto unbrs = query_graph->getVertexNeighbors(u, unbr_cnt);
    ui valid_cans_cnt = cans_cnt[u];

    // 遍历 u 在查询图中的每一个邻居 unbr
    for (ui unbr_idx = 0; unbr_idx < unbr_cnt; unbr_idx++) {
        auto& unbr = unbrs[unbr_idx];

        // 获取当前 u 的所有候选点在数据图中的邻居信息
        for (ui can_idx = 0; can_idx < cans_cnt[u]; can_idx++) {
            auto u_can = cans[u][can_idx];
            u_cans_nbrs[can_idx] = global->getNeighbors(u, unbr, u_can, u_cans_nbrs_cnt[can_idx]);
        }

        // 利用 SetOp 检查半连接约束 (Semi-join constraint)d
        // 检查 u 的候选点是否与 unbr 的候选点存在连边
        SetOp::multi_overlap_no_alloc(u_cans_nbrs, u_cans_nbrs_cnt, valid_cans_cnt,
                                      cans[unbr], cans_cnt[unbr],
                                      results_buffer, aux_cursors, aux_queue);

        // 3. 根据结果压缩 cans[u] 数组
        ui new_valid = 0;
        for (ui valid_can_idx = 0; valid_can_idx < valid_cans_cnt; valid_can_idx++) {
            if (results_buffer[valid_can_idx] == true) {
                cans[u][new_valid] = cans[u][valid_can_idx];
                new_valid++;
            } else {
                cans_cnt[u]--;
            }
        }
        valid_cans_cnt = new_valid;
    }
    cans_cnt[u] = valid_cans_cnt;
    return valid_cans_cnt;
}

void
CSMIndex::buildCSMEdgeMatrix(CSMIndex* global) {
    ui q_num = query_graph->getVerticesCount();
    dnum = global->dnum;
    d_edge_num = global->d_edge_num;
    ui* flag = new ui[dnum];
    ui* updated_flag = new ui[dnum];
    fill(flag, flag + dnum, 0);

    for (ui i = 0; i < q_num; ++i) {
        for (ui j = 0; j < q_num; ++j) {
            edge_matrix[i][j] = nullptr;
        }
    }

    vector<VertexID> build_table_order(q_num);
    for (ui i = 0; i < q_num; ++i) {
        build_table_order[i] = i;
    }

    sort(build_table_order.begin(), build_table_order.end(), [this](VertexID l, VertexID r) {
        if (query_graph->getVertexDegree(l) == query_graph->getVertexDegree(r)) {
            return l < r;
        }
        return query_graph->getVertexDegree(l) > query_graph->getVertexDegree(r);
    });

    vector<ui> temp_edges(d_edge_num * 2);

    for (auto u : build_table_order) {
        ui u_nbrs_count;
        const VertexID* u_nbrs = query_graph->getVertexNeighbors(u, u_nbrs_count);
        ui updated_flag_count = 0;

        for (ui i = 0; i < u_nbrs_count; ++i) {
            VertexID u_nbr = u_nbrs[i];
            if (edge_matrix[u][u_nbr] != nullptr)
                continue;

            if (updated_flag_count == 0) {
                for (ui j = 0; j < cans_cnt[u]; ++j) {
                    VertexID v = cans[u][j];
                    flag[v] = j + 1;
                    updated_flag[updated_flag_count++] = v;
                }
            }

            edge_matrix[u_nbr][u] = new Edges;
            edge_matrix[u_nbr][u]->vertex_count_ = cans_cnt[u_nbr];
            edge_matrix[u_nbr][u]->offset_ = new ui[cans_cnt[u_nbr] + 1];

            edge_matrix[u][u_nbr] = new Edges;
            edge_matrix[u][u_nbr]->vertex_count_ = cans_cnt[u];
            edge_matrix[u][u_nbr]->offset_ = new ui[cans_cnt[u] + 1];
            fill(edge_matrix[u][u_nbr]->offset_, edge_matrix[u][u_nbr]->offset_ + cans_cnt[u] + 1, 0);

            ui local_edge_count = 0;
            ui local_max_degree = 0;

            for (ui j = 0; j < cans_cnt[u_nbr]; ++j) {
                VertexID v = cans[u_nbr][j];
                edge_matrix[u_nbr][u]->offset_[j] = local_edge_count;

                ui v_nbrs_count;
                const VertexID* v_nbrs = global->getNeighbors(u_nbr, u, v, v_nbrs_count);
                ui local_degree = 0;

                for (ui k = 0; k < v_nbrs_count; ++k) {
                    VertexID v_nbr = v_nbrs[k];
                    if (flag[v_nbr] != 0) {
                        ui position = flag[v_nbr] - 1;
                        temp_edges[local_edge_count++] = v_nbr;  // record id. instead of idx
                        edge_matrix[u][u_nbr]->offset_[position + 1] += 1;
                        local_degree += 1;
                    }
                }

                if (local_degree > local_max_degree) {
                    local_max_degree = local_degree;
                }
            }

            edge_matrix[u_nbr][u]->offset_[cans_cnt[u_nbr]] = local_edge_count;
            edge_matrix[u_nbr][u]->max_degree_ = local_max_degree;
            edge_matrix[u_nbr][u]->edge_count_ = local_edge_count;
            edge_matrix[u_nbr][u]->edge_ = new ui[local_edge_count];
            copy(temp_edges.begin(), temp_edges.begin() + local_edge_count, edge_matrix[u_nbr][u]->edge_);

            edge_matrix[u][u_nbr]->edge_count_ = local_edge_count;
            edge_matrix[u][u_nbr]->edge_ = new ui[local_edge_count];

            local_max_degree = 0;
            for (ui j = 1; j <= cans_cnt[u]; ++j) {
                if (edge_matrix[u][u_nbr]->offset_[j] > local_max_degree) {
                    local_max_degree = edge_matrix[u][u_nbr]->offset_[j];
                }
                edge_matrix[u][u_nbr]->offset_[j] += edge_matrix[u][u_nbr]->offset_[j - 1];
            }

            edge_matrix[u][u_nbr]->max_degree_ = local_max_degree;

            for (ui j = 0; j < cans_cnt[u_nbr]; ++j) {
                VertexID begin = cans[u_nbr][j];
                for (ui k = edge_matrix[u_nbr][u]->offset_[j]; k < edge_matrix[u_nbr][u]->offset_[j + 1]; ++k) {
                    ui end_idx = flag[edge_matrix[u_nbr][u]->edge_[k]] - 1;
                    edge_matrix[u][u_nbr]->edge_[edge_matrix[u][u_nbr]->offset_[end_idx]++] = begin;
                }
            }

            for (ui j = cans_cnt[u]; j >= 1; --j) {
                edge_matrix[u][u_nbr]->offset_[j] = edge_matrix[u][u_nbr]->offset_[j - 1];
            }
            edge_matrix[u][u_nbr]->offset_[0] = 0;
        }

        for (ui i = 0; i < updated_flag_count; ++i) {
            VertexID v = updated_flag[i];
            flag[v] = 0;
        }
    }
    delete[] flag;
    delete[] updated_flag;
}

// get Neighbors of v(can of u_1) from u_1 to u_2
// used to generate valid cans, by union nbrs, 24-3-7
const VertexID* CSMIndex::getNeighbors(VertexID u_1, VertexID u_2, VertexID v, ui& nbrs_cnt) {
    auto v_idx = b_search::lower_bound_idx(cans[u_1], cans_cnt[u_1], v);
    if (v_idx == cans_cnt[u_1]  || cans[u_1][v_idx] != v) {
        nbrs_cnt = 0;
        return nullptr;
    }
    auto& edges = *(edge_matrix[u_1][u_2]);
    nbrs_cnt = edges.offset_[v_idx+1] - edges.offset_[v_idx];
    return edges.edge_ + edges.offset_[v_idx];
}

inline void CSMIndex::copy_block(ui* dst, const ui* src, ui len) {
    if (len > 0 && src != nullptr && dst != nullptr) {
        copy(src, src + len, dst);
    }
}