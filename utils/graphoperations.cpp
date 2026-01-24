#include "graphoperations.h"
#include <memory.h>
#include <queue>

void GraphOperations::getKCore(const Graph *graph, int *core_table) {
    int vertices_count = graph->getVerticesCount();
    int max_degree = graph->getGraphMaxDegree();

    int* vertices = new int[vertices_count];          // Vertices sorted by degree.
    int* position = new int[vertices_count];          // The position of vertices in vertices array.
    int* degree_bin = new int[max_degree + 1];      // Degree from 0 to max_degree.
    int* offset = new int[max_degree + 1];          // The offset in vertices array according to degree.

    std::fill(degree_bin, degree_bin + (max_degree + 1), 0);

    for (int i = 0; i < vertices_count; ++i) {
        int degree = graph->getVertexDegree(i);
        core_table[i] = degree;
        degree_bin[degree] += 1;
    }

    int start = 0;
    for (int i = 0; i < max_degree + 1; ++i) {
        offset[i] = start;
        start += degree_bin[i];
    }

    for (int i = 0; i < vertices_count; ++i) {
        int degree = graph->getVertexDegree(i);
        position[i] = offset[degree];
        vertices[position[i]] = i;
        offset[degree] += 1;
    }

    for (int i = max_degree; i > 0; --i) {
        offset[i] = offset[i - 1];
    }
    offset[0] = 0;

    for (int i = 0; i < vertices_count; ++i) {
        int v = vertices[i];

        ui count;
        const VertexID * neighbors = graph->getVertexNeighbors(v, count);

        for(int j = 0; j < count; ++j) {
            int u = neighbors[j];

            if (core_table[u] > core_table[v]) {

                // Get the position and vertex which is with the same degree
                // and at the start position of vertices array.
                int cur_degree_u = core_table[u];
                int position_u = position[u];
                int position_w = offset[cur_degree_u];
                int w = vertices[position_w];

                if (u != w) {
                    // Swap u and w.
                    position[u] = position_w;
                    position[w] = position_u;
                    vertices[position_u] = w;
                    vertices[position_w] = u;
                }

                offset[cur_degree_u] += 1;
                core_table[u] -= 1;
            }
        }
    }

    delete[] vertices;
    delete[] position;
    delete[] degree_bin;
    delete[] offset;
}

void GraphOperations::old_cheap(int* col_ptrs, int* col_ids, int* match, int* row_match, int n, int m) {
    int ptr;
    int i = 0;
    for(; i < n; i++) {
        int s_ptr = col_ptrs[i];
        int e_ptr = col_ptrs[i + 1];
        for(ptr = s_ptr; ptr < e_ptr; ptr++) {
            int r_id = col_ids[ptr];
            if(row_match[r_id] == -1) {
                match[i] = r_id;
                row_match[r_id] = i;
                break;
            }
        }
    }
}

void GraphOperations::match_bfs(int* col_ptrs, int* col_ids, int* match, int* row_match, int* visited,
                        int* queue, int* previous, int n, int m) {
    int queue_ptr, queue_col, ptr, next_augment_no, i, j, queue_size,
            row, col, temp, eptr;

    old_cheap(col_ptrs, col_ids, match, row_match, n, m);

    memset(visited, 0, sizeof(int) * m);

    next_augment_no = 1;
    for(i = 0; i < n; i++) {
        if(match[i] == -1 && col_ptrs[i] != col_ptrs[i+1]) {
            queue[0] = i; queue_ptr = 0; queue_size = 1;

            while(queue_size > queue_ptr) {
                queue_col = queue[queue_ptr++];
                eptr = col_ptrs[queue_col + 1];
                for(ptr = col_ptrs[queue_col]; ptr < eptr; ptr++) {
                    row = col_ids[ptr];
                    temp = visited[row];

                    if(temp != next_augment_no && temp != -1) {
                        previous[row] = queue_col;
                        visited[row] = next_augment_no;

                        col = row_match[row];

                        if(col == -1) {
                            // Find an augmenting path. Then, trace back and modify the augmenting path.
                            while(row != -1) {
                                col = previous[row];
                                temp = match[col];
                                match[col] = row;
                                row_match[row] = col;
                                row = temp;
                            }
                            next_augment_no++;
                            queue_size = 0;
                            break;
                        } else {
                            // Continue to construct the match.
                            queue[queue_size++] = col;
                        }
                    }
                }
            }

            if(match[i] == -1) {
                for(j = 1; j < queue_size; j++) {
                    visited[match[queue[j]]] = -1;
                }
            }
        }
    }
}

void GraphOperations::bfsTraversal(const Graph *graph, VertexID root_vertex, TreeNode *&tree, VertexID *&bfs_order) {
    ui vertex_num = graph->getVerticesCount();

    std::queue<VertexID> bfs_queue;
    std::vector<bool> visited(vertex_num, false);

    tree = new TreeNode[vertex_num];
    for (ui i = 0; i < vertex_num; ++i) {
        tree[i].initialize(vertex_num);
    }
    bfs_order = new VertexID[vertex_num];

    ui visited_vertex_count = 0;
    bfs_queue.push(root_vertex);
    visited[root_vertex] = true;
    tree[root_vertex].level_ = 0;
    tree[root_vertex].id_ = root_vertex;

    while(!bfs_queue.empty()) {
        const VertexID u = bfs_queue.front();
        bfs_queue.pop();
        bfs_order[visited_vertex_count++] = u;

        ui u_nbrs_count;
        const VertexID* u_nbrs = graph->getVertexNeighbors(u, u_nbrs_count);
        for (ui i = 0; i < u_nbrs_count; ++i) {
            VertexID u_nbr = u_nbrs[i];

            if (!visited[u_nbr]) {
                bfs_queue.push(u_nbr);
                visited[u_nbr] = true;
                tree[u_nbr].id_ = u_nbr;
                tree[u_nbr].parent_ = u;
                tree[u_nbr].level_ = tree[u] .level_ + 1;
                tree[u].children_[tree[u].children_count_++] = u_nbr;
            }
        }
    }
}

void GraphOperations::dfsTraversal(TreeNode *tree, VertexID root_vertex, ui node_num, VertexID *&dfs_order) {
    dfs_order = new VertexID[node_num];
    ui count = 0;
    dfs(tree, root_vertex, dfs_order, count);
}

void GraphOperations::dfs(TreeNode *tree, VertexID cur_vertex, VertexID *dfs_order, ui &count) {
    dfs_order[count++] = cur_vertex;

    for (ui i = 0; i < tree[cur_vertex].children_count_; ++i) {
        dfs(tree, tree[cur_vertex].children_[i], dfs_order, count);
    }
}

void GraphOperations::compute_automorphism(const Graph *graph, std::vector<std::vector<uint32_t>> &embeddings) {
    // Note that this method is working on small graphs (tens of vertices) only.
    // Initialize resource.
    uint32_t n = graph->getVerticesCount();
    std::vector<bool> visited(n, false);
    std::vector<uint32_t> idx(n);
    std::vector<uint32_t> mapping(n);
    std::vector<std::vector<uint32_t>> local_candidates(n);
    std::vector<std::vector<uint32_t>> global_candidates(n);
    std::vector<std::vector<uint32_t>> backward_neighbors(n);

    // Initialize global candidates.
    for (uint32_t u = 0; u < n; ++u) {
        uint32_t u_label = graph->getVertexLabel(u);
        uint32_t u_degree = graph->getVertexDegree(u);

        for (uint32_t v = 0; v < n; ++v) {
            uint32_t v_label = graph->getVertexLabel(v);
            uint32_t v_degree = graph->getVertexDegree(v);

            if (v_label == u_label && v_degree >= u_degree)
                global_candidates[u].push_back(v);
        }
    }

    // Generate a matching order.
    std::vector<uint32_t> matching_order;
    uint32_t selected_vertex = 0;
    uint32_t selected_vertex_selectivity = global_candidates[selected_vertex].size();
    for (uint32_t u = 1; u < n; ++u) {
        if (global_candidates[u].size() < selected_vertex_selectivity){
            selected_vertex = u;
            selected_vertex_selectivity = global_candidates[u].size();
        }
    }

    matching_order.push_back(selected_vertex);
    visited[selected_vertex] = true;

    for (uint32_t i = 1; i < n; ++i) {
        selected_vertex_selectivity = n + 1;
        for (uint32_t u = 0; u < n; ++u) {
            if (!visited[u]) {
                bool is_feasible = false;

                uint32_t u_nbr_count;
                auto u_nbr = graph->getVertexNeighbors(u, u_nbr_count);
                for (uint32_t j = 0; j < u_nbr_count; ++j) {
                    uint32_t uu = u_nbr[j];

                    if (visited[uu]) {
                        is_feasible = true;
                        break;
                    }
                }

                if (is_feasible && global_candidates[u].size() < selected_vertex_selectivity) {
                    selected_vertex = u;
                    selected_vertex_selectivity = global_candidates[u].size();
                }
            }
        }
        matching_order.push_back(selected_vertex);
        visited[selected_vertex] = true;
    }

    std::fill(visited.begin(), visited.end(), false);

    // Set backward neighbors to compute local candidates.
    for (uint32_t i = 1; i < n; ++i) {
        uint32_t u = matching_order[i];
        for (uint32_t j = 0; j < i; ++j) {
            uint32_t uu = matching_order[j];

            if (graph->checkEdgeExistence(uu, u)) {
                backward_neighbors[u].push_back(uu);
            }
        }
    }

    // Recursive search along the matching order.
    int cur_level = 0;
    local_candidates[cur_level] = global_candidates[matching_order[0]];

    while (true) {
        while (idx[cur_level] < local_candidates[cur_level].size()) {
            uint32_t u = matching_order[cur_level];
            uint32_t v = local_candidates[cur_level][idx[cur_level]];
            idx[cur_level] += 1;

            if (cur_level == n - 1) {
                // Find an embedding
                mapping[u] = v;
                embeddings.push_back(mapping);
            } else {
                mapping[u] = v;
                visited[v] = true;
                cur_level += 1;
                idx[cur_level] = 0;

                {
                    // Compute local candidates.
                    u = matching_order[cur_level];
                    for (auto temp_v: global_candidates[u]) {
                        if (!visited[temp_v]) {
                            bool is_feasible = true;
                            for (auto uu: backward_neighbors[u]) {
                                uint32_t temp_vv = mapping[uu];
                                // TODO: check edge label
                                if (!graph->checkEdgeExistence(temp_v, temp_vv)) {
                                    is_feasible = false;
                                    continue;
                                }
                            }
                            if (is_feasible)
                                local_candidates[cur_level].push_back(temp_v);
                        }
                    }
                }
            }
        }

        local_candidates[cur_level].clear();
        cur_level -= 1;
        if (cur_level < 0) {
            break;
        }
        visited[mapping[matching_order[cur_level]]] = false;
    }
}


