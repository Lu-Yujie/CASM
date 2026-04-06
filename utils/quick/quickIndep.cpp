#include "quickIndep.h"
#include <cstring>
#include <algorithm>

using namespace std;

QuickIndep::QuickIndep(ui dnum, const Graph* query_graph, bool* visited_u) {
    this->qnum = query_graph->getVerticesCount();
    this->query_graph = query_graph;
    indep_con_cnt = new ui[dnum];

    // Allocate Shared Buffers (Max size is qnum)
    degree = new ui[qnum];
    enum_cans = new const VertexID*[qnum];
    enum_cans_cnt = new ui[qnum];
    enum_idx = new ui[qnum];
    un_con_cnt = new ui[qnum];
    phase = new bool[qnum];
    embedding_level = new mpz_t[qnum];
    for (ui i = 0; i < qnum; i++) mpz_init(embedding_level[i]);

    // Precompute Independent Sets for All Edges
    initAllEdges(visited_u);
}

QuickIndep::~QuickIndep() {
    delete[] indep_con_cnt;
    delete[] degree;
    for (auto& pair : edge_infos) delete[] pair.second.cover_set;
    delete[] enum_cans;
    delete[] enum_cans_cnt;
    delete[] enum_idx;
    delete[] un_con_cnt;
    delete[] phase;

    for (ui i = 0 ; i < indep_num; i++) mpz_clear(embedding_level[i]);
    delete[] embedding_level;
}

void QuickIndep::initAllEdges(bool* visited_u) {
    auto offset = query_graph->getOffsets();
    auto edges = query_graph->getEdges();

    for (VertexID u = 0; u < qnum; u++) {
        for (ui i = offset[u]; i < offset[u + 1]; i++) {
            VertexID v = edges[i];

            // Only process each undirected edge once
            if (u < v) { 
                EdgeIndepInfo info;
                // Allocate qnum space for the combined cover_set + indep_set
                info.cover_set = new VertexID[qnum]; 

                buildIndepForEdge(query_graph, u, v, info, visited_u);
                edge_infos[{u, v}] = info;
            }
        }
    }
}

void QuickIndep::print() {
    std::cout << "=== Edge Independent Sets Info ===" << std::endl;
    for (const auto& pair : edge_infos) {
        VertexID u = pair.first.first;
        VertexID v = pair.first.second;
        const auto& info = pair.second;

        std::cout << "Edge (" << u << ", " << v << "):" << std::endl;
        
        // 打印 Cover Set
        std::cout << "  Cover Set (" << info.cover_num << "): [";
        for (ui i = 0; i < info.cover_num; i++) {
            std::cout << info.cover_set[i] << (i == info.cover_num - 1 ? "" : ", ");
        }
        std::cout << "]" << std::endl;

        // 打印 Independent Set
        std::cout << "  Indep Set (" << info.indep_num << "): [";
        for (ui i = 0; i < info.indep_num; i++) {
            std::cout << info.indep_set[i] << (i == info.indep_num - 1 ? "" : ", ");
        }
        std::cout << "]" << std::endl;
    }
    std::cout << "==================================" << std::endl;
    // exit(-1);
}

void QuickIndep::buildIndepForEdge(const Graph* graph, VertexID src, VertexID dst, EdgeIndepInfo& info, bool* visited_u) {
    auto offset = graph->getOffsets();
    auto edges = graph->getEdges();

    // Reset visited flags and calculate degrees
    memset(visited_u, false, sizeof(bool) * qnum);
    for (VertexID i = 0; i < qnum; i++) {
        degree[i] = graph->getVertexDegree(i);
    }

    info.cover_num = 0;

    // Helper lambda to fix a node into cover_set and update degrees
    auto fix_node = [&](VertexID u) {
        if (!visited_u[u]) {
            visited_u[u] = true;
            info.cover_set[info.cover_num++] = u;
            for (ui i = offset[u]; i < offset[u + 1]; i++) {
                VertexID neighbor = edges[i];
                if (degree[neighbor] > 0) degree[neighbor]--;
            }
        }
    };

    // 1. Force fix the edge endpoints into cover_set first
    fix_node(src);
    fix_node(dst);

    // 2. Select remaining cover set based on remaining Degree (High -> Low)
    while (true) {
        int maxDegree = 0;
        VertexID selectedNode = (VertexID)-1;
        for (VertexID i = 0; i < qnum; i++) {
            if (!visited_u[i] && degree[i] > maxDegree) {
                maxDegree = degree[i];
                selectedNode = i;
            }
        }

        if (selectedNode == (VertexID)-1) break;
        fix_node(selectedNode);
    }

    // 3. The remaining unvisited nodes become the independent set
    info.indep_num = 0;
    info.indep_set = info.cover_set + info.cover_num;
    for (VertexID i = 0; i < qnum; i++) {
        if (!visited_u[i]) {
            info.indep_set[info.indep_num++] = i;
        }
    }
}

void QuickIndep::set_cur(VertexID v_src, VertexID v_dst) {
    // Standardize edge key (undirected representation)
    VertexID u = std::min(v_src, v_dst);
    VertexID v = std::max(v_src, v_dst);

    auto it = edge_infos.find({u, v});
    if (it != edge_infos.end()) {
        indep_set = it->second.indep_set;
        cover_set = it->second.cover_set;
        indep_num = it->second.indep_num;
        cover_num = it->second.cover_num;
    } else {
        cout << "can not find matched indep info" << endl;
        exit(-1);
    }
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
    if (UNLIKELY(indep_num == 0)) {
        mpz_set_ui(embedding_level[0], 1); 
        return;
    }

    ui depth = 0;
    auto& idx = enum_idx;
    auto& cans = enum_cans;
    auto& cans_cnt = enum_cans_cnt;

    idx[0] = 0;
    phase[0] = true;
    mpz_set_ui(embedding_level[0], 0);

    while (true) {
        // --- 最后一个节点 ---
        if (depth == indep_num - 1) {
            ui tmp_cnt = 0;
            ui last_size = cans_cnt[depth];
            const VertexID* last_cans = cans[depth];
            for (ui i = 0; i < last_size; i++) {
                if (!visited_v[last_cans[i]]) tmp_cnt++;
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