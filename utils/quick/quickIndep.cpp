#include "quickIndep.h"
#include <cstring>
#include <algorithm>

using namespace std;

QuickIndep::QuickIndep(ui dnum, const Graph* query_graph, bool* visited_u) {
    auto& qnum = query_graph->getVerticesCount();
    this->query_graph = query_graph;
    indep_con_cnt = new ui[dnum];
    memset(indep_con_cnt, 0, sizeof(ui)*dnum);

    sep_flag = new ui[qnum];
    degree = new ui[qnum];

    cover_set = new VertexID[qnum];
    indepSetOnDegree(query_graph, cover_set, visited_u);
    indep_set = cover_set+cover_num;

    enum_cans = new const VertexID*[indep_num];
    enum_cans_cnt = new ui[indep_num];
    enum_idx = new ui[indep_num];
    enum_cnt = new ui[indep_num];
    un_con_cnt = new ui[indep_num];
    local_cans_ptrs_.resize(indep_num);

    embedding_level = new mpz_t[indep_num];
    for (ui i = 0 ; i < indep_num; i++) mpz_init(embedding_level[i]);
}

QuickIndep::~QuickIndep() {
    delete[] indep_con_cnt;
    delete[] sep_flag;
    delete[] degree;
    delete[] cover_set;
    delete[] enum_cans;
    delete[] enum_cans_cnt;
    delete[] enum_idx;
    delete[] enum_cnt;
    delete[] un_con_cnt;

    for (ui i = 0 ; i < indep_num; i++) mpz_clear(embedding_level[i]);
    delete[] embedding_level;
}

void QuickIndep::enumeration(bool* visited_v) {
    // 2. 计算所需 buffer 总大小 并 填充节点列表
    size_t total_buffer_needed = 0;
    for (ui i = 0; i < indep_num; i++) {
        total_buffer_needed += enum_cans_cnt[i];
    }

    // 3. 扩容 buffer
    if (local_cans_buffer_.size() < total_buffer_needed) {
        local_cans_buffer_.resize(total_buffer_needed);
    }

    // 4. 全局极速拷贝 (Memcpy)
    ui* buffer_cursor = local_cans_buffer_.data();
    for (ui i = 0; i < indep_num; i++) {
        ui cnt = enum_cans_cnt[i];
        local_cans_ptrs_[i] = buffer_cursor; 
        std::memcpy(buffer_cursor, enum_cans[i], cnt * sizeof(VertexID));
        buffer_cursor += cnt;
    }

    // ----------------------------------------------------------------
    // Pass 1: Count conflicts
    for (ui i = 0; i < indep_num; i++) {
        ui* v_cans = local_cans_ptrs_[i];
        ui cnt = enum_cans_cnt[i];
        for (ui j = 0; j < cnt; j++) {
            indep_con_cnt[v_cans[j]]++;
        }
    }

    // Pass 2: Partition
    for (ui i = 0; i < indep_num; i++) {
        ui* v_cans = local_cans_ptrs_[i];
        ui cnt = enum_cans_cnt[i];
        auto& downward_sep0 = sep_flag[i];   

        for (ui j = 0; j < cnt; j++) {
            indep_con_cnt[v_cans[j]]--;
        }

        // 划分：如果在剩余节点中计数仍 > 0，说明后面还有人要用它，归为 Conflict 部分
        downward_sep0 = sepDiff(v_cans, indep_con_cnt, 0, (int)cnt - 1);
    }

    // Pass 3: Enumerate (一次性递归所有节点)
    enum4Parts(visited_v);
}

// ----------------------------------------------------------------------
// 分区辅助函数 (纯指针操作，无 vector)
ui QuickIndep::sepDiff(ui* v_cans, const ui *indep_con_cnt, int forward_idx, int backward_idx) {
    if (backward_idx - forward_idx == 0) return indep_con_cnt[v_cans[forward_idx]] != 0;

    ui first_con_cnt = indep_con_cnt[v_cans[forward_idx]];
    VertexID first_val = v_cans[forward_idx];
    while(forward_idx < backward_idx) {
        while(forward_idx < backward_idx && !indep_con_cnt[v_cans[backward_idx]]) backward_idx--;
        if (forward_idx < backward_idx) v_cans[forward_idx++] = v_cans[backward_idx];

        while(forward_idx < backward_idx && indep_con_cnt[v_cans[forward_idx]]) forward_idx++;
        if (forward_idx < backward_idx) v_cans[backward_idx--] = v_cans[forward_idx];
    }

    v_cans[forward_idx] = first_val;
    if (first_con_cnt) forward_idx++;
    return forward_idx;
}

// ----------------------------------------------------------------------
// 递归枚举核心 (纯指针操作)
void QuickIndep::enum4Parts(bool* visited_v) {
    ui depth = 0;

    auto& idx = enum_idx;
    auto& cnt = enum_cnt;
    auto& cans = local_cans_ptrs_;
    auto& cans_cnt = enum_cans_cnt;

    // 初始化第一层
    idx[depth] = 0;
    // 如果 sep_flag 有效，先搜冲突部分；否则全搜
    cnt[depth] = sep_flag[0] < cans_cnt[0] ? sep_flag[0] + 1 : cans_cnt[0];

    mpz_set_ui(embedding_level[depth], 0);
    un_con_cnt[depth] = 0;

    while (true) {
        while (idx[depth] < cnt[depth]) {
            VertexID v = cans[depth][idx[depth]]; 
            ui& cur_sep = sep_flag[depth];

            if (depth == indep_num - 1) {
                // Leaf Node: 直接统计剩余可用候选
                ui tmp_cnt = 0;
                ui leaf_size = cans_cnt[depth];
                ui* leaf_cans = cans[depth];
                for (ui i = 0; i < leaf_size; i++) {
                    if (!visited_v[leaf_cans[i]]) tmp_cnt++;
                }
                mpz_add_ui(embedding_level[depth], embedding_level[depth], tmp_cnt);
                break; 
            } else {
                idx[depth]++;

                // 判断是否进入了 "无冲突块" (Optimization Block)
                if (idx[depth] > cur_sep) {
                    ui current_size = cans_cnt[depth];
                    ui* current_cans = cans[depth];

                    for (ui i = cur_sep; i < current_size; i++) {
                        if (!visited_v[current_cans[i]]) un_con_cnt[depth]++;
                    }
                    // 如果没有可用候选，此分支结束
                    if (un_con_cnt[depth] == 0) break;

                } else {
                    // Standard DFS (冲突部分)
                    if (visited_v[v]) continue;
                    visited_v[v] = true;
                }

                // 进入下一层
                depth++;
                idx[depth] = 0;

                // 默认限制为搜索冲突部分 + 1个触发无冲突计算的入口
                cnt[depth] = sep_flag[depth] < cans_cnt[depth] ? sep_flag[depth] + 1 : cans_cnt[depth];
                
                mpz_set_ui(embedding_level[depth], 0);
                un_con_cnt[depth] = 0;
            }
        }
        
        depth--;
        if (depth == (ui)-1) break;

        // 回溯处理
        if (idx[depth] > sep_flag[depth]) {
            // "无冲突块" 的计算逻辑: 当前层的 un_con_cnt * 下一层的方案数
            mpz_mul_ui(embedding_level[depth+1], embedding_level[depth+1], un_con_cnt[depth]);
        } else {
            // "普通 DFS" 的回溯
            VertexID v = cans[depth][idx[depth]-1];
            visited_v[v] = false;
        }

        // 累加子树结果到当前层
        mpz_add(embedding_level[depth], embedding_level[depth], embedding_level[depth+1]);
    }

    // 最终结果在 embedding_level[0]
}

// Select independent set based on Degree (Greedy)
void QuickIndep::indepSetOnDegree(const Graph* graph, VertexID *order, bool* visited_u) {
    ui n = graph->getVerticesCount();
    auto offset = graph->getOffsets();
    auto edges = graph->getEdges();

    for (VertexID i = 0; i < n; i++) {
        if (graph->getVertexDegree(i) > 1)
            degree[i] = graph->getVertexDegree(i);
        else 
            degree[i] = 0; 
    }

    cover_num = 0;
    while (true) {
        int maxDegree = 0, selectedNode = (VertexID)-1;
        for (VertexID i = 0; i < n; i++) {
            if (!visited_u[i] && degree[i] > maxDegree) {
                maxDegree = degree[i];
                selectedNode = i;
            }
        }

        if (selectedNode == (VertexID)-1) break;

        visited_u[selectedNode] = true;
        order[cover_num++] = selectedNode;

        // Reduce neighbors' degrees
        for (ui i = offset[selectedNode]; i < offset[selectedNode + 1]; i++) {
            VertexID neighbor = edges[i];
            if(degree[neighbor] > 0) degree[neighbor]--;
        }
    }

    indep_num = 0;
    for (VertexID i = 0; i < n; i++) {
        if (!visited_u[i]) {
            order[indep_num + cover_num] = i;
            indep_num++;
        }
    }

    return;
}

// Select independent set based on Min Cans -> Max Degree
void QuickIndep::indepSetOnCans(const Graph* graph, VertexID *order, ui* cans_cnt, bool* visited_u) {
    ui n = graph->getVerticesCount();
    auto offset = graph->getOffsets();
    auto edges = graph->getEdges();

    for (VertexID i = 0; i < n; i++) degree[i] = graph->getVertexDegree(i);

    cover_num = 0;
    while (true) {
        ui maxDegree = 0, minCans = (ui)-1;
        VertexID selectedNode = (VertexID)-1;
        
        for (VertexID i = 0; i < n; i++) {
            if (!visited_u[i] && degree[i] > 0) {
                if (cans_cnt[i] < minCans || (cans_cnt[i] == minCans && maxDegree < degree[i])) {
                    minCans = cans_cnt[i];
                    maxDegree = degree[i];
                    selectedNode = i;
                }
            }
        }

        if (selectedNode == (VertexID)-1) break;

        visited_u[selectedNode] = true;
        order[cover_num++] = selectedNode;

        for (ui i = offset[selectedNode]; i < offset[selectedNode + 1]; i++) {
            VertexID neighbor = edges[i];
            if (degree[neighbor] > 0) degree[neighbor]--; // 增加了 >0 的判断，防止 ui(无符号整型) 下溢出
        }
    }

    // --- 补充独立集的点到 cover 的后面 ---
    indep_num = 0;
    for (VertexID i = 0; i < n; i++) {
        if (!visited_u[i]) {
            order[indep_num + cover_num] = i;
            indep_num++;
        }
    }

    return;
}

// Heuristic: Min Cans -> Max Degree, strictly excluding degree <= 1
void QuickIndep::indepSetOnCansExcludeDegreeOne(const Graph* graph, VertexID *order, ui* cans_cnt, bool* visited_u) {
    ui n = graph->getVerticesCount();
    auto offset = graph->getOffsets();
    auto edges = graph->getEdges();

    for (VertexID i = 0; i < n; i++) degree[i] = graph->getVertexDegree(i);

    cover_num = 0;
    while (true) {
        VertexID selectedNode = (VertexID)-1;
        ui maxCans = 0;
        ui maxDegree = 0;

        for (VertexID i = 0; i < n; i++) {
            // Ignore leaf nodes (degree <= 1)
            if (!visited_u[i] && degree[i] > 1) {
                bool isBetter = false;
                if (selectedNode == (VertexID)-1) {
                    isBetter = true;
                } else {
                    // Priority 1: More candidates is better (unusual heuristic, but matches original logic)
                    if (cans_cnt[i] > maxCans) {  
                        isBetter = true;
                    } else if (cans_cnt[i] == maxCans && degree[i] > maxDegree) { 
                        isBetter = true;
                    }
                }

                if (isBetter) {
                    selectedNode = i;
                    maxCans = cans_cnt[i];
                    maxDegree = degree[i];
                }
            }
        }

        if (selectedNode == (VertexID)-1) break;

        visited_u[selectedNode] = true;
        order[cover_num++] = selectedNode;

        for (ui i = offset[selectedNode]; i < offset[selectedNode + 1]; i++) {
            VertexID neighbor = edges[i];
            if (degree[neighbor] > 0) degree[neighbor]--;
        }
    }

    // --- 补充独立集的点到 cover 的后面 ---
    indep_num = 0;
    for (VertexID i = 0; i < n; i++) {
        if (!visited_u[i]) {
            order[indep_num + cover_num] = i;
            indep_num++;
        }
    }

    return;
}