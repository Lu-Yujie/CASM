#include "QueryEngine.h"
#include "utils/bsx/IndepSet.h"
#include "utils/setOp.h"
#include <stack>
#include <vector>
#include <cstring>
#include <sys/stat.h>
#include <timeOp.h>

void
QueryEngine::QuickEngine(CSMIndex* csm_index, size_t output_limit_num,
                         mpz_t embedding_cnt, int64_t& time_limit) {
    auto& query_graph = csm_index->mem->q_graph;
    auto& qnum = csm_index->mem->q_num;

    auto& quick_index = *(csm_index->mem->quick_index);
    auto& valid_cans = quick_index.valid_cans;
    auto& valid_idx = quick_index.valid_idx;
    auto& visited_u = quick_index.visited_u;
    auto& visited_v = quick_index.visited_v;
    auto& influenced_cnt = quick_index.influenced_u_cnt;

    auto& embedding = quick_index.embedding;
    auto& u2v = quick_index.embedding->u2v;

    auto& indep_info = *(quick_index.indep_info);
    auto& cover_set = indep_info.cover_set;
    auto& cover_num = indep_info.cover_num;
    auto& indep_embeddings = indep_info.embedding_level[0];

    auto& local_cans = csm_index->cans;
    for (ui i = 0; i < qnum; i++) {
        valid_cans[i].importRootCandidates(local_cans[i].data(), local_cans[i].size());
    }

    mpz_set_ui(embedding_cnt, 0);
    ui cur_depth = 0;
    VertexID start_vertex = cover_set[cur_depth];
    memset(visited_u, false, sizeof(bool)*qnum);
    visited_u[start_vertex] = true;
    valid_idx[start_vertex] = 0;

    while (true) {
        while (valid_idx[cover_set[cur_depth]] < valid_cans[cover_set[cur_depth]].cur_cans_cnt()) {
            if (TimeOp::getClockNan() >= time_limit) {
                goto EXIT;
            }
            VertexID u = cover_set[cur_depth];
            VertexID v = valid_cans[u].cur_cans()[valid_idx[u]];
            // cout << "u: " << u << ", v: " << v << endl;
            valid_idx[u]++;
            if (visited_v[v]) continue;

            VertexID failed_u = quickRefine(csm_index, u, v);
            if (failed_u != (ui)-1) {  // no valid cans for next depth
                // cout << "u: " << u << ", failed_u: " << failed_u << ", fail" << endl;
                continue;
            }

            u2v[u] = v;
            visited_v[v] = true;

            if (cur_depth >= cover_num - 1) {
                // enumerate results on indep nodes, process ancestors' ves by the way
                quickEnum(quick_index);
                // gmp_printf("new result: %Zd\n", level_embeddings);
                mpz_add(embedding_cnt, embedding_cnt, indep_embeddings);
                // next batch
                quickDeRefine(csm_index, u);
                visited_v[v] = false;
            } else {
                cur_depth++;
                VertexID nxt_u = cover_set[cur_depth];
                valid_idx[nxt_u] = 0;
                visited_u[nxt_u] = true;
            }
        }

        // backtracking
        cur_depth -= 1;
        if (cur_depth == ui(-1))
            break;
        auto& last_u = cover_set[cur_depth+1];
        auto& cur_u = cover_set[cur_depth];
        visited_u[last_u] = false;
        visited_v[u2v[cur_u]] = false;

        quickDeRefine(csm_index, cur_u);
    }

    EXIT:
    // Release the buffer.
    return;
}

ui
QueryEngine::quickRefine(CSMIndex* csm_index, VertexID u, VertexID v) {
    auto& qnum = csm_index->mem->q_num;
    auto& query_graph = csm_index->mem->q_graph;
    auto& quick_index = *(csm_index->mem->quick_index);
    auto& valid_cans = quick_index.valid_cans;
    auto& visited_u = quick_index.visited_u;
    auto& visited_v = quick_index.visited_v;

    auto& influenced = quick_index.influenced_u[u];
    auto& influenced_cnt = quick_index.influenced_u_cnt[u];
    influenced_cnt = 0;
    ui unbrs_cnt;
    auto unbrs = query_graph->getVertexNeighbors(u, unbrs_cnt);
    for (ui i = 0; i < unbrs_cnt; i++) {
        auto& unbr = unbrs[i];
        if (visited_u[unbr]) continue;
        // old valid_cans of unbr
        auto& vnbrs = csm_index->aux.getNeighbors(u, unbr, v);
        auto u_cans = valid_cans[unbr].cur_cans();
        auto u_cans_cnt = valid_cans[unbr].cur_cans_cnt();
        auto u_nxt_cans = valid_cans[unbr].next_buffer();
        auto& u_next_cans_cnt = valid_cans[unbr].next_buffer_cnt();
        u_next_cans_cnt = SetOp::intersectTwo(vnbrs, u_cans, u_cans_cnt, u_nxt_cans);
        if (u_next_cans_cnt == 0) {
            influenced_cnt = 0;
            return unbr;
        }

        if (u_cans_cnt == u_next_cans_cnt) continue;  // no change, no push
        influenced[influenced_cnt++] = unbr;  // cnt is cleared when visited_u is set
    }
    for (ui i = 0; i < influenced_cnt; i++) {
        valid_cans[influenced[i]].push();
    }

    return (ui)-1;
}

/**Reverse op of BSXRefine
 * pop the valid_cans of influenced_u
 * process oneCansV from refinement
*/
void
QueryEngine::quickDeRefine(CSMIndex* csm_index, VertexID u) {
    auto& quick_index = *(csm_index->mem->quick_index);
    auto& valid_cans = quick_index.valid_cans;
    auto& influenced = quick_index.influenced_u[u];
    auto& influenced_cnt = quick_index.influenced_u_cnt[u];

    for (ui i = 0; i < influenced_cnt; i++) {
        auto& influenced_u = influenced[i];
        valid_cans[influenced_u].pop();
    }
}

void
QueryEngine::quickEnum(QuickIndex& index) {
    auto& nodes = index.indep_info->indep_set;
    auto& nodes_num = index.indep_info->indep_num;
    auto& enum_cans = index.indep_info->enum_cans;
    auto& enum_cans_cnt = index.indep_info->enum_cans_cnt;
    for (ui i = 0; i < nodes_num; i++) {
        enum_cans[i] = index.valid_cans[nodes[i]].cur_cans();
        enum_cans_cnt[i] = index.valid_cans[nodes[i]].cur_cans_cnt();
    }
    // extract the candidates of indep vertices
    index.indep_info->enumeration(index.visited_v);

    return;
}

/**
 * use bsx method
*/
void
QueryEngine::BSXEngine(ui d_num, const Graph *query_graph, Edges ***edge_matrix,
                          ui **candidates, ui *candidates_count, ui *order, ISEIndex* isei,
                          size_t output_limit_num, mpz_t embedding_cnt, int64_t& time_limit) {
    ui q_num = query_graph->getVerticesCount();
    // separate leaf and trunk vertices(min_vertex_cover)
    ui num_cover = 0;
    bsxMaxCoverOrder(query_graph, order, num_cover, candidates_count);
    auto num_indep = q_num - num_cover;
    const VertexID* indep_nodes =  order + num_cover;

    BSXIndex index(query_graph, d_num, edge_matrix, candidates, candidates_count, num_cover, isei);
    auto& batch_info = index.batch_info;

    // auxiliary data structure
    auto& visited_u = index.visited_u;
    auto& u2v = index.embedding->u2v;
    auto& depth2u = index.embedding->depth2u;
    mpz_set_ui(embedding_cnt, 0);
    ui cur_depth = 0;
    VertexID start_vertex = order[cur_depth];
    depth2u.emplace_back(start_vertex);
    visited_u[start_vertex] = true;
    auto& level_embeddings = index.level_embeddings_;

    // init info of start vertex
    batch_info[start_vertex].add();
    bsxComEqBatch(index, start_vertex);
    // batch_info[start_vertex].print();
    index.valid_cans_[start_vertex].push(new VertexID[batch_info[start_vertex].maxCnt_.top()]);
    index.valid_cnt_[start_vertex].push(0);

    while (true) {
        while (batch_info[depth2u[cur_depth]].idx_.top() < batch_info[depth2u[cur_depth]].num_.top()) {
            VertexID u = depth2u[cur_depth];
            ui cur_batch_cnt;
            VertexID* cur_batch = batch_info[u].cur_batch(cur_batch_cnt);
            if (TimeOp::getClockNan() >= time_limit) {
                goto EXIT;
            }

            // nxt batch
            batch_info[depth2u[cur_depth]].idx_.top()++;

            auto& cur_cans_cnt = index.valid_cnt_[u].top();
            auto& cur_cans = index.valid_cans_[u].top();
            std::copy(cur_batch, cur_batch+cur_batch_cnt, cur_cans);
            cur_cans_cnt = cur_batch_cnt;

            VertexID failed_u = bsxRefine(index, u);
            if (failed_u != (ui)-1) {  // no valid cans for next depth
                continue;
            }

            u2v[u] = cur_batch[0];

            if (cur_depth >= num_cover - 1) {
                // enumerate results on indep nodes, process ancestors' ves by the way
                bsxGenResult(num_indep, indep_nodes, index);
                mpz_add(embedding_cnt, embedding_cnt, level_embeddings);
                if (output_limit_num != (size_t)-1 && mpz_cmp_ui(embedding_cnt, output_limit_num) > 0) {
                    goto EXIT;
                }
                // next batch
                bsxDeRefine(index);
            } else {
                cur_depth++;
                VertexID cur_u = bsxGenNxtU(index, order, cur_depth, num_cover);
                if (cur_u == (VertexID)-1) cur_u = order[cur_depth];
                depth2u.emplace_back(cur_u);
                // construct nbrs&seperate batches, and then refinement
                batch_info[cur_u].add();
                bsxComEqBatch(index, cur_u);
                // batch_info[cur_u].print();
                index.valid_cans_[cur_u].push(new VertexID[batch_info[cur_u].maxCnt_.top()]);
                index.valid_cnt_[cur_u].push(0);
                visited_u[cur_u] = true;
            }
        }

        // backtracking
        cur_depth -= 1;
        if (cur_depth == ui(-1))
            break;
        VertexID last_u = depth2u[cur_depth+1];
        depth2u.resize(cur_depth+1);
        visited_u[last_u] = false;

        batch_info[last_u].pop();
        delete[] index.valid_cans_[last_u].top();
        index.valid_cans_[last_u].pop();
        index.valid_cnt_[last_u].pop();
        bsxDeRefine(index);
    }

    // Release the buffer.
    EXIT:
    return;
}

/**
 * generate order for BSXEngine
 * 1. static: seperate vertices into cover&independent by MaxCover
 * 2. dynamic: sort each kind by #cans(asc), #degree(des), #id(asc)
 * compute static order by max indep cover, dynamic order computed along matching
*/
void
QueryEngine::bsxMaxCoverOrder(const Graph *graph, ui *&order, ui& num_cover, ui *candidates_count) {
    auto q_num = graph->getVerticesCount();
    if (order == nullptr) {
        order = new ui[q_num];
    }
    IndepSet indepSet(graph);
    auto indep = indepSet.linearTime();
    num_cover = q_num - indep.second;
    // complete order
    ui num_indep = 0;
    ui num_other = 0;
    for (ui i = 0; i < q_num; i++) {
        if (indep.first[i]) {
            order[num_cover + num_indep++] = i;
        } else {
            order[num_other++] = i;
        }
    }
    // compute the first u
    ui first_u = order[0];
    ui first_idx = 0;
    for (ui i = 1; i < num_cover; i++) {
        ui cur_u = order[i];
        if (candidates_count[cur_u] < candidates_count[first_u]
            || (candidates_count[cur_u] == candidates_count[first_u]
                && graph->getVertexDegree(cur_u) > graph->getVertexDegree(first_u))) {
            first_idx = i;
            first_u = cur_u;
        }
    }
    order[first_idx] = order[0];
    order[0] = first_u;

    // dynamic->static order
    // std::sort(order, order+num_cover,[candidates_count, graph](VertexID l, VertexID r){
    //     if (candidates_count[l] == candidates_count[r]) {
    //         if (graph->getVertexDegree(l) == graph->getVertexDegree(r)) {
    //             return l < r;  // id(asc)
    //         }
    //         return graph->getVertexDegree(l) > graph->getVertexDegree(r);  // degree(desc)
    //     }
    //     return candidates_count[l] < candidates_count[r];  // cans(asc)
    // });
    delete[] indep.first;
}

/**Reverse op of BSXRefine
 * pop the valid_cans of influenced_u
 * process oneCansV from refinement
*/
void
QueryEngine::bsxDeRefine(BSXIndex& index) {
    auto q_num = index.q_graph_->getVerticesCount();
    bool* nbr_updated = new bool[q_num];
    std::copy(index.visited_u, index.visited_u+q_num, nbr_updated);

    // process first u in influenced_u seperately, valid_cans of first_influenced_u comes from
    //   its batch_nodes, and couldn't be deleted
    auto first_u = index.influenced_u_.top()[0];
    // remove index_, added at refinement
    ui nbrs_cnt;
    auto nbrs = index.q_graph_->getVertexNeighbors(first_u, nbrs_cnt);
    for (ui i = 0; i < nbrs_cnt; i++) {
        auto& nbr = nbrs[i];
        if (nbr_updated[nbr]) continue;
        delete index.index_[nbr][first_u].top();
        delete index.index_[first_u][nbr].top();
        index.index_[nbr][first_u].pop();
        index.index_[first_u][nbr].pop();
    }
    // recover index_cans_, changed at refinement
    auto tmp_cans = index.valid_cans_[first_u].top();
    index.valid_cans_[first_u].pop();
    index.valid_cnt_[first_u].pop();
    index.index_cans_[first_u] = index.valid_cans_[first_u].top();
    index.index_cnt_[first_u] = index.valid_cnt_[first_u].top();
    index.valid_cans_[first_u].push(tmp_cans);
    index.valid_cnt_[first_u].push(0);

    for (ui i = 1; i < index.influenced_u_.top().size(); i++) {
        auto influenced_u = index.influenced_u_.top()[i];
        // not delete first influenced_u, because its valid_cans is not constructed in refine
        delete[] index.valid_cans_[influenced_u].top();
        index.valid_cans_[influenced_u].pop();
        index.valid_cnt_[influenced_u].pop();
        index.index_cans_[influenced_u] = index.valid_cans_[influenced_u].top();
        index.index_cnt_[influenced_u] = index.valid_cnt_[influenced_u].top();

        // remove index_
        ui nbrs_cnt;
        auto nbrs = index.q_graph_->getVertexNeighbors(influenced_u, nbrs_cnt);
        for (ui i = 0; i < nbrs_cnt; i++) {
            auto& nbr = nbrs[i];
            if (nbr_updated[nbr]) continue;
            delete index.index_[nbr][influenced_u].top();
            delete index.index_[influenced_u][nbr].top();
            index.index_[nbr][influenced_u].pop();
            index.index_[influenced_u][nbr].pop();
        }
        nbr_updated[influenced_u] = true;
    }

    index.influenced_u_.pop();
    delete[] nbr_updated;
}

/**generate next u
 * sort by:
 * 1.depth < num_cover: not_matched, #cans(asc), #degree(des), #id(arbitrary)
 * 2.depth >= num_cover: #cans!=1, #cans(asc), #degree(des), #id(arbitrary)
*/
VertexID
QueryEngine::bsxGenNxtU(BSXIndex& index, VertexID* order, ui depth, ui num_cover) {
    auto& valid_cnt = index.valid_cnt_;
    auto& graph = index.q_graph_;
    if (depth < num_cover) {
        std::sort(order+depth, order+num_cover, [valid_cnt, graph](VertexID a, VertexID b) {
            if (valid_cnt[a].top() == valid_cnt[b].top()) {
                return graph->getVertexDegree(a) > graph->getVertexDegree(b);
            }
            return valid_cnt[a].top() < valid_cnt[b].top();
        });
        return (VertexID)-1;
    } else {
        VertexID* tmp_nodes = new VertexID[num_cover];
        std::copy(order,order+num_cover, tmp_nodes);
        std::sort(tmp_nodes, tmp_nodes+num_cover, [valid_cnt, graph](ui a, ui b) {
            if (valid_cnt[a].top() != 1 && valid_cnt[b].top() != 1) {
                if (valid_cnt[a].top() == valid_cnt[b].top()) {
                    return graph->getVertexDegree(a) > graph->getVertexDegree(b);
                }
                return valid_cnt[a].top() < valid_cnt[b].top();
            }
            return valid_cnt[a].top() > valid_cnt[b].top();
        });
        VertexID node = tmp_nodes[0];
        delete[]tmp_nodes;
        return node;
    }
    return (VertexID)-1;
}

// check termination (each no-indep only one cans)
bool
QueryEngine::bsxCheckTermination(ui num, VertexID* indep, std::stack<ui>*valid_cnt) {
    while (num) if (valid_cnt[indep[--num]].top() != 1) return false;
    return true;
}

// compute valid_cans for all indep, detect conflict
bool
QueryEngine::bsxGenIndepValidCans(ui indep_num, const VertexID* indep, BSXIndex& index, std::vector<std::vector<VertexID>>& cans) {
    const VertexID** uu_nbrs = new const VertexID*[index.q_graph_->getVerticesCount()];
    ui* uu_nbrs_cnt = new  ui[index.q_graph_->getVerticesCount()];
    for (ui i = 0; i < indep_num; i++) {
        auto u = indep[i];
        ui nbrs_cnt = 0;
        auto nbrs = index.q_graph_->getVertexNeighbors(u, nbrs_cnt);
        for (ui j = 0; j < nbrs_cnt; j++) {
            auto& nbr = nbrs[j];
            auto& nbr_v = index.embedding->u2v[nbr];
            uu_nbrs[j] = index.getNeighbors(nbr, u, nbr_v, uu_nbrs_cnt[j]);
        }
        auto intersected = std::move(SetOp::intersectMultiple(uu_nbrs, uu_nbrs_cnt, nbrs_cnt));
        if (intersected.size() == 0) {
            delete[] uu_nbrs;
            delete[] uu_nbrs_cnt;
            return false;
        }
        cans[u] = std::move(intersected);
    }
    delete[] uu_nbrs;
    delete[] uu_nbrs_cnt;
    return true;
}

// generate equivalent batches
void
QueryEngine::bsxComEqBatch(BSXIndex& index, VertexID u) {
    auto& num_node = index.valid_cnt_[u].top();
    auto& batch_nodes = index.batch_info[u].nodes_.top();
    auto& offset = index.batch_info[u].offset_.top();
    auto& cnt = index.batch_info[u].cnt_.top();
    auto& isei = index.isei_;

    batch_nodes = new VertexID[num_node];
    offset = new VertexID[num_node];
    cnt = new VertexID[num_node];

    auto& nodes = index.valid_cans_[u].top();
    ui unbrs_count;
    const ui *unbrs = index.q_graph_->getVertexNeighbors(u, unbrs_count);

    // 过滤无效点
    thread_local std::vector<VertexID> filtered_vids;
    thread_local std::vector<ui> filtered_original_idxs;
    filtered_vids.clear();
    filtered_original_idxs.clear();

    for (ui i = 0; i < num_node; i++) {
        bool is_valid = true;
        for (ui unbr_idx = 0; unbr_idx < unbrs_count; unbr_idx++) {
            auto& edges = index.index_[u][unbrs[unbr_idx]].top();
            if (edges->offset_[i+1] - edges->offset_[i] == 0) {
                is_valid = false; break;
            }
        }
        if (is_valid) {
            filtered_vids.push_back(nodes[i]);
            filtered_original_idxs.push_back(i);
        }
    }
    if (filtered_vids.empty()) return;

    // 调用 ISEI 获取预分组辅助结构
    const auto& pre_group = isei->getPreGroup(filtered_vids.data(), filtered_vids.size());

    // 对每一个“特征相同块”执行精细化校验
    ui current_pos = 0;
    ui total_vids = pre_group.size();
    while (current_pos < total_vids) {
        ui block_start = current_pos;
        uint64_t current_sig = pre_group[block_start].signature;
        while (current_pos < total_vids && pre_group[current_pos].signature == current_sig) {
            current_pos++;
        }

        // 精细分组
        bsxComEqBatchDirect(index, u, pre_group, block_start, current_pos, filtered_original_idxs);
    }
}

// compute equ-batch on idxs, idxs indicate which nodes participate batch computation
void
QueryEngine::bsxComEqBatchDirect(BSXIndex& index, VertexID u, const std::vector<CandidateSig>& pre_group,
                                 ui start, ui end, const std::vector<ui>& filtered_original_idxs) {
    auto& batch_nodes = index.batch_info[u].nodes_.top();
    auto& offset = index.batch_info[u].offset_.top();
    auto& cnt = index.batch_info[u].cnt_.top();
    auto& num = index.batch_info[u].num_.top();
    auto& maxCnt = index.batch_info[u].maxCnt_.top();

    auto& nodes = index.valid_cans_[u].top();
    ui unbrs_count;
    const ui *unbrs = index.q_graph_->getVertexNeighbors(u, unbrs_count);

    // 块内局部处理
    ui block_size = end - start;
    thread_local std::vector<bool> local_assigned;
    local_assigned.assign(block_size, false);

    for (ui i = 0; i < block_size; i++) {
        if (local_assigned[i]) continue;

        // 提取节点在 BSX 原始索引中的真正位置
        ui filtered_idx_i = pre_group[start + i].original_idx;
        ui real_idx_i = filtered_original_idxs[filtered_idx_i];

        offset[num] = num == 0 ? 0 : offset[num-1] + cnt[num-1];
        cnt[num] = 1;
        batch_nodes[offset[num]] = nodes[real_idx_i];
        local_assigned[i] = true;

        for (ui j = i + 1; j < block_size; j++) {
            if (local_assigned[j]) continue;

            ui filtered_idx_j = pre_group[start + j].original_idx;
            ui real_idx_j = filtered_original_idxs[filtered_idx_j];

            // 邻居校验
            bool equ = true;
            for (ui unbr_idx = 0; unbr_idx < unbrs_count; unbr_idx++) {
                auto& edges = index.index_[u][unbrs[unbr_idx]].top();
                ui d1 = edges->offset_[real_idx_i+1] - edges->offset_[real_idx_i];
                ui d2 = edges->offset_[real_idx_j+1] - edges->offset_[real_idx_j];
                if (d1 != d2) { equ = false; break; }
                
                ui s1 = edges->offset_[real_idx_i], s2 = edges->offset_[real_idx_j];
                for (ui k = 0; k < d1; k++) {
                    if (edges->edge_[s1+k] != edges->edge_[s2+k]) { equ = false; break; }
                }
                if (!equ) break;
            }

            if (equ) {
                local_assigned[j] = true;
                batch_nodes[offset[num] + (cnt[num]++)] = nodes[real_idx_j];
            }
        }
        std::sort(batch_nodes + offset[num], batch_nodes + offset[num] + cnt[num]);
        if (maxCnt < cnt[num]) maxCnt = cnt[num];
        num++;
    }
}

// equ-batch refine, just process first v of valid_cans, because of they are equ
// if success, return -1, else return failed uId
ui
QueryEngine::bsxRefine(BSXIndex& index, VertexID u) {
    ui q_num = index.q_graph_->getVerticesCount();
    auto v = index.valid_cans_[u].top()[0];
    bool* influenced = new bool[q_num];
    memset(influenced, false, sizeof(bool)*q_num);
    std::vector<VertexID> cur_inf;
    // if a node is influenced for the first time, just use the generated result
    // do not need intersection operation(op) with old valid_cans
    std::pair<const VertexID*, ui>* influenced_cans = new std::pair<const VertexID*, ui>[q_num];
    ui unbrs_cnt;
    auto unbrs = index.q_graph_->getVertexNeighbors(u, unbrs_cnt);
    ui returned_value = (ui)-1;
    for (ui i = 0; i < unbrs_cnt; i++) {
        auto& unbr = unbrs[i];
        if (index.visited_u[unbr]) continue;
        // old valid_cans of unbr
        auto& unbr_valid_cnt = index.valid_cnt_[unbr].top();
        ui vnbr_cnt;
        auto vnbrs = index.getNeighbors(u, unbr, v, vnbr_cnt);
        if (vnbr_cnt == 0) {
            returned_value = unbr;
            goto bsxRefine_EXIT;
        }
        // // vnbrs must be included in unbr_valid_cans
        // assert(SetOp::setInclude(vnbrs, vnbr_cnt, unbr_valid_cans, unbr_valid_cnt));

        if (unbr_valid_cnt == vnbr_cnt) continue;  // means no changes

        // stack a valid_cans&valid_cnt
        influenced[unbr] = true;
        influenced_cans[unbr] = std::make_pair(vnbrs, vnbr_cnt);
    }

    // write influenced valid_cans to index, then update index
    for (ui unbr = 0; unbr < q_num; unbr++) {
        if (influenced[unbr]) {
            auto& vnbr_cnt = influenced_cans[unbr].second;
            auto& vnbrs = influenced_cans[unbr].first;
            auto new_valid_cans = new VertexID[vnbr_cnt];
            std::copy(vnbrs, vnbrs+vnbr_cnt, new_valid_cans);
            index.valid_cans_[unbr].push(new_valid_cans);
            index.valid_cnt_[unbr].push(vnbr_cnt);
        }
    }
    influenced[u] = true;

    // implement index.updateOneSide(influenced) later
    index.updateIndex(influenced, u);
    // if (index.updateIndex(influenced) == (ui)-1) {
    //     influenced[u] = false;
    //     for (ui i = 0; i < q_num; i++) {
    //         if (influenced[i]) {
    //             delete[] index.valid_cans_[i].top();
    //             index.valid_cans_[i].pop();
    //             index.valid_cnt_[i].pop();
    //         }
    //     }
    //     returned_value = (ui)-1;
    //     goto VESREFINE_EXIT;
    // }

    // update valid_cans to index_cans, which is used for edges(index.index_)
    influenced[u] = false;
    index.index_cans_[u] = index.valid_cans_[u].top();
    index.index_cnt_[u] = index.valid_cnt_[u].top();
    cur_inf.emplace_back(u);
    for (ui i = 0; i < q_num; i++) {
        if (influenced[i]) {
            index.index_cans_[i] = index.valid_cans_[i].top();
            index.index_cnt_[i] = index.valid_cnt_[i].top();
            cur_inf.emplace_back(i);
        }
    }
    index.influenced_u_.emplace(std::move(cur_inf));

    bsxRefine_EXIT:
    delete[] influenced;
    delete[] influenced_cans;
    return returned_value;
}

void
QueryEngine::bsxGenResult(ui indep_num, const VertexID* indep, BSXIndex& index) {
    auto& visited_v = index.visited_v;
    auto& indep_con_cnt = index.indep_con_cnt_;
    auto& sep_flag = index.sep_flag_;  // indexed by idx
    auto& embedding_cnt = index.level_embeddings_;
    auto& label_embeddings = index.label_embeddings_;
    auto qnum = index.q_graph_->getVerticesCount();
    auto label_num = index.q_graph_->getLabelsCount();
    // generate valid_cans of indeps
    std::vector<std::vector<VertexID>> cans;
    cans.resize(qnum);
    for (ui i = 0; i < index.num_cover_; i++) {
        auto u = index.embedding->depth2u[i];
        cans[u].reserve(index.valid_cnt_[u].top());
        for (ui j = 0; j < index.valid_cnt_[u].top(); j++) {
            cans[u].emplace_back(index.valid_cans_[u].top()[j]);
        }
    }
    if (bsxGenIndepValidCans(indep_num, indep, index, cans) == false) return;
    mpz_set_ui(embedding_cnt, 1);
    for (ui l_idx = 0; l_idx < label_num; l_idx++) {
        ui nodes_num;
        // these nodes have the same label
        auto nodes = index.q_graph_->getVerticesByLabel(l_idx, nodes_num);
        if (nodes_num == 0) continue;
        // compute the number of valid embedding
        // 1.By intersected, compute the cans which may conflict with others
        // 2.Based on conflict info, seperate cans into two part
        //   con: may conflict with others nodes, process as a backtracking
        //   un-con: will not conflict with others, use (#un-con) * (#embeddings of down-level)
        //   con&un-con->all need process visited_v
        // 3.Order does not matter in this backtracking, and will not influence up-level
        // ** because there is no great idea to process up-coflict nodes,
        //    we do not seperate nodes base on up-conlict
        if (nodes_num == 1) {  // the order of indep are always the same
            mpz_mul_ui(embedding_cnt, embedding_cnt, cans[nodes[0]].size());
            continue;
        }
        // 1.first scan, compute upward conflict
        for (ui i = 0; i < nodes_num; i++) {
            auto& node = nodes[i];
            auto& v_cans = cans[node];
            // do not seperate nodes base on up-conlict
            // auto& upward_sep = sep_flag[cur_idx][1];
            // auto v_cans_cnt = v_cans.size();
            // int forward_idx = 0;
            // int backward_idx = v_cans_cnt - 1;  // backward_idx may be -1
            // upward_sep = bsxSepDiff(v_cans, indep_con_cnt, forward_idx, backward_idx);
            for (auto v_can:v_cans) indep_con_cnt[v_can]++;
        }
        // 2.second scan, compute downward conflict
        for (ui i = 0; i < nodes_num; i++) {
            auto& node = nodes[i];
            auto& v_cans = cans[node];
            auto& downward_sep0 = sep_flag[node][0];  // 0->sep the up-conflicts
            // auto& downward_sep1 = sep_flag[cur_idx][2];  // 1->sep the up-uncon.
            auto v_cans_cnt = v_cans.size();  // assert(v_cans_cnt > 0) check at cans generation
            int forward_idx = 0;
            // int middle_idx = sep_flag[cur_idx][1];
            // int backward_idx = v_cans_cnt - 1;
            for (auto v_can:v_cans) indep_con_cnt[v_can]--;
            downward_sep0 = bsxSepDiff(v_cans, indep_con_cnt, forward_idx, v_cans_cnt - 1);
            // downward_sep1 = bsxSepDiff(v_cans, indep_con_cnt, middle_idx, backward_idx);
        }
        // 3.enumerate the nodes based on diff features of 4 parts
        // ** just 2 parts so far
        bsxEnumerate4Parts(sep_flag, nodes, nodes_num, cans, visited_v, label_embeddings);
        mpz_mul(embedding_cnt, embedding_cnt, label_embeddings);
        if (mpz_cmp_ui(embedding_cnt, 0) == 0) return;
    }

    return;
}

// according to indep_con_cnt info, seperate v_cans into two parts, return the #first_part(true)
ui
QueryEngine::bsxSepDiff(std::vector<VertexID> &v_cans, const ui *indep_con_cnt, int forward_idx, int backward_idx) {
    if (backward_idx-forward_idx == 0) return indep_con_cnt[v_cans[forward_idx]] != 0;
    ui first_con_cnt = indep_con_cnt[v_cans[forward_idx]];
    VertexID first_idx = v_cans[forward_idx];
    while(forward_idx < backward_idx) {
        while(forward_idx < backward_idx && !indep_con_cnt[v_cans[backward_idx]]) backward_idx--;
        if (forward_idx < backward_idx)
            v_cans[forward_idx++] = v_cans[backward_idx];
        while(forward_idx < backward_idx && indep_con_cnt[v_cans[forward_idx]]) forward_idx++;
        if (forward_idx < backward_idx)
            v_cans[backward_idx--] = v_cans[forward_idx];
    }
    v_cans[forward_idx] = first_idx;
    if (first_con_cnt) forward_idx++;
    return forward_idx;
}

// TODO: opt to three parts
void  // 4 parts: up-down,up-x,x-down,x-x; down&x 2 parts so far
QueryEngine::bsxEnumerate4Parts(ui **&sep_flags, const VertexID* nodes, ui nodes_num,
                                   std::vector<std::vector<VertexID>>& cans, bool *&visited_v,
                                   mpz_t cur_cnt) {
    ui depth = 0;
    ui* idx = new ui[nodes_num];
    ui* cnt = new ui[nodes_num];
    ui* un_con_cnt = new ui[nodes_num];
    idx[depth] = 0;
    cnt[depth] = sep_flags[nodes[depth]][0] < cans[nodes[depth]].size()
                 ? sep_flags[nodes[depth]][0] + 1 : cans[nodes[depth]].size();
    mpz_t* embedding_level = new mpz_t[nodes_num];
    for (ui i = 0; i < nodes_num; i++) {
        mpz_init(embedding_level[i]);
    }
    mpz_set_ui(embedding_level[depth], 0);
    un_con_cnt[depth] = 0;
    while (true) {
        while (idx[depth] < cnt[depth]) {
            auto u_idx = nodes[depth];
            VertexID& v = cans[u_idx][idx[depth]];
            ui& cur_sep = sep_flags[u_idx][0];
            if (depth == nodes_num - 1) {
                ui tmp_cnt = 0;
                for (ui i = 0; i < cans[u_idx].size(); i++) {
                    if (!visited_v[cans[u_idx][i]]) tmp_cnt++;
                }
                mpz_add_ui(embedding_level[depth], embedding_level[depth], tmp_cnt);
                break;
            } else {
                idx[depth]++;
                if (idx[depth] > cur_sep) {
                    for (ui i = cur_sep; i < cans[u_idx].size(); i++) {
                        auto& can = cans[u_idx][i];
                        if (!visited_v[can]) un_con_cnt[depth]++;
                    }
                    if (un_con_cnt[depth] == 0) break;
                } else {
                    if (visited_v[v]) continue;
                    visited_v[v] = true;
                }
                depth++;
                idx[depth] = 0;
                cnt[depth] = sep_flags[nodes[depth]][0] + 1;
                mpz_set_ui(embedding_level[depth], 0);
                un_con_cnt[depth] = 0;
            }
        }
        depth--;
        if (depth == (ui)-1) {
            break;
        }
        if (idx[depth] > sep_flags[nodes[depth]][0]) {
            // process nodes which will not conflict downward
            mpz_mul_ui(embedding_level[depth+1], embedding_level[depth+1], un_con_cnt[depth]);
        } else {
            // process visited_v
            VertexID& v = cans[nodes[depth]][idx[depth]-1];
            visited_v[v] = false;
        }
        mpz_add(embedding_level[depth], embedding_level[depth], embedding_level[depth+1]);
    }

    delete[] idx;
    delete[] cnt;
    delete[] un_con_cnt;
    mpz_set(cur_cnt, embedding_level[0]);
    for (ui i = 0; i < nodes_num; i++) {
        mpz_clear(embedding_level[i]);
    }
    delete[] embedding_level;
    return;
}
