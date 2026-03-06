#include "quickIndep.h"
#include <cstring>
#include <algorithm>

using namespace std;

QuickIndep::QuickIndep(ui dnum, const Graph* query_graph, bool* visited_u) {
    auto& qnum = query_graph->getVerticesCount();
    this->query_graph = query_graph;
    indep_con_cnt = new ui[dnum];
    memset(indep_con_cnt, 0, sizeof(ui)*dnum);

    degree = new ui[qnum];

    cover_set = new VertexID[qnum];
    indepSetOnDegree(query_graph, cover_set, visited_u);
    indep_set = cover_set+cover_num;

    enum_cans = new const VertexID*[indep_num];
    enum_cans_cnt = new ui[indep_num];
    enum_idx = new ui[indep_num];
    un_con_cnt = new ui[indep_num];
    phase = new bool[indep_num];

    embedding_level = new mpz_t[indep_num];
    for (ui i = 0 ; i < indep_num; i++) mpz_init(embedding_level[i]);
}

QuickIndep::~QuickIndep() {
    delete[] indep_con_cnt;
    delete[] degree;
    delete[] cover_set;
    delete[] enum_cans;
    delete[] enum_cans_cnt;
    delete[] enum_idx;
    delete[] un_con_cnt;
    delete[] phase;

    for (ui i = 0 ; i < indep_num; i++) mpz_clear(embedding_level[i]);
    delete[] embedding_level;
}

void QuickIndep::enumeration(bool* visited_v) {
    // Count conflicts
    for (ui i = 0; i < indep_num; i++) {
        ui cnt = enum_cans_cnt[i];
        const VertexID* cans = enum_cans[i];
        for (ui j = 0; j < cnt; j++) {
            indep_con_cnt[cans[j]] = i; 
        }
    }

    enum4Parts(visited_v);
}

void QuickIndep::enum4Parts(bool* visited_v) {
    if (indep_num == 0) return;

    ui depth = 0;
    auto& idx = enum_idx;
    auto& cans = enum_cans;
    auto& cans_cnt = enum_cans_cnt;

    idx[0] = 0;
    phase[0] = true;
    mpz_set_ui(embedding_level[0], 0);

    while (true) {
        // --- 叶子节点逻辑保持不变 ---
        if (depth == indep_num - 1) {
            ui tmp_cnt = 0;
            ui leaf_size = cans_cnt[depth];
            const VertexID* leaf_cans = cans[depth];
            for (ui i = 0; i < leaf_size; i++) {
                if (!visited_v[leaf_cans[i]]) tmp_cnt++;
            }
            mpz_add_ui(embedding_level[depth], embedding_level[depth], tmp_cnt);
            goto BACKTRACK;
        }

        // --- Phase 1: 优先处理所有冲突节点 (常规 DFS) ---
        if (phase[depth]) {
            bool pushed = false;
            while (idx[depth] < cans_cnt[depth]) {
                VertexID v = cans[depth][idx[depth]];
                idx[depth]++;
                
                // 如果最深层级 > 当前深度，说明后续还会用到，是冲突节点
                if (indep_con_cnt[v] > depth) {
                    if (visited_v[v]) continue;
                    
                    visited_v[v] = true;
                    
                    // 下钻到下一层
                    depth++;
                    idx[depth] = 0;
                    phase[depth] = true;
                    mpz_set_ui(embedding_level[depth], 0);
                    pushed = true;
                    break;
                }
            }
            if (pushed) continue; // 成功下钻，回到 while(true) 开始下一层

            // --- Phase 1 结束，平滑进入 Phase 2 ---
            phase[depth] = false;
            un_con_cnt[depth] = 0;
            
            // 统计无冲突节点数量
            ui cur_size = cans_cnt[depth];
            const VertexID* cur_cans = cans[depth];
            for (ui i = 0; i < cur_size; i++) {
                VertexID v = cur_cans[i];
                // 如果最深层级 == 当前深度，说明这是最后一次出现，是无冲突节点
                if (indep_con_cnt[v] == depth && !visited_v[v]) {
                    un_con_cnt[depth]++;
                }
            }

            if (un_con_cnt[depth] > 0) {
                // 无冲突节点触发优化：无需 mark visited，只需派出一个代表下钻一次
                depth++;
                idx[depth] = 0;
                phase[depth] = true;
                mpz_set_ui(embedding_level[depth], 0);
                continue;
            }
            // 如果连无冲突节点都没有，直接 fall-through 进回溯
        }

    BACKTRACK:
        depth--;
        if (depth == (ui)-1) break;

        if (phase[depth]) {
            // 从冲突节点的 DFS 中回溯，恢复 visited 状态
            VertexID v = cans[depth][idx[depth] - 1];
            visited_v[v] = false;
            mpz_add(embedding_level[depth], embedding_level[depth], embedding_level[depth+1]);
        } else {
            // 从无冲突节点的优化分支中回溯，结果乘以可选数量
            mpz_mul_ui(embedding_level[depth+1], embedding_level[depth+1], un_con_cnt[depth]);
            mpz_add(embedding_level[depth], embedding_level[depth], embedding_level[depth+1]);
        }
    }
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