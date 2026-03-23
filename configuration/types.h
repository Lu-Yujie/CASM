#ifndef CSM_TYPES_H
#define CSM_TYPES_H

#include <cstdint>
#include <stdlib.h>
#include <functional>
#include <vector>
#include <algorithm>
#include <absl/container/flat_hash_map.h>
#include "utils/bSearch.h"

using namespace std;

typedef unsigned int ui;
typedef uint32_t VertexID;
typedef ui LabelID;

struct ELabel {
    LabelID src_label_;
    LabelID dst_label_;
    bool operator==(const ELabel& l) const {
        return l.src_label_ == src_label_ && l.dst_label_ == dst_label_;
    }
    ELabel(LabelID src_label, LabelID dst_label): src_label_(src_label), dst_label_(dst_label) {}
    ELabel(): src_label_(0), dst_label_(0) {}
};

struct Edge {
    VertexID src_, dst_;
    ELabel elabel_;
    Edge(VertexID src, VertexID dst, LabelID src_label = 0, LabelID dst_label = 0):
        elabel_(src_label, dst_label) {
        src_ = src;
        dst_ = dst;
    }
    Edge() {}
    bool operator==(const Edge& l) const {
        return l.src_ == src_ && l.dst_ == dst_ && l.elabel_ == elabel_;
    }
    VertexID& src() { return src_; }
    VertexID& dst() { return dst_; }
};

namespace std {
    template <>
    struct hash<ELabel> {
        size_t operator()(const ELabel& k) const {
            size_t h1 = std::hash<uint32_t>()(k.src_label_);
            size_t h2 = std::hash<uint32_t>()(k.dst_label_);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
    template <>
    struct hash<Edge> {
        size_t operator()(const Edge& k) const {
            size_t h1 = std::hash<uint32_t>()(k.src_);
            size_t h2 = std::hash<uint32_t>()(k.dst_);
            h1 = h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
            h2 = std::hash<ELabel>()(k.elabel_);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
}

struct Update {
    uint64_t id_;
    char op_;
    Edge edge_;
    VertexID& src() { return edge_.src_; }
    VertexID& dst() { return edge_.dst_; }
    LabelID& src_label() { return edge_.elabel_.src_label_; }
    LabelID& dst_label() { return edge_.elabel_.dst_label_; }
};

class Edges {
public:
    ui* offset_;
    ui* edge_;
    ui vertex_count_;
    ui edge_count_;
public:
    Edges() {
        offset_ = nullptr;
        edge_ = nullptr;
        vertex_count_ = 0;
        edge_count_ = 0;
    }

    ~Edges() {
        delete[] offset_;
        delete[] edge_;
    }
};

struct CSMEdges {
    absl::flat_hash_map<VertexID, std::vector<VertexID>> edge_map;
    CSMEdges() {}
    ~CSMEdges() {}

    inline const std::vector<VertexID>* get_neighbors(VertexID u) const {
        auto it = edge_map.find(u);
        if (it != edge_map.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    inline void add_edge(VertexID u, VertexID v) {
        auto& neighbors = edge_map[u];
        if (neighbors.empty() || neighbors.back() < v) {
            neighbors.push_back(v);
        } else {
            ui idx = b_search::lower_bound_idx(neighbors, v);
            if (idx == neighbors.size() || neighbors[idx] != v) {
                neighbors.insert(neighbors.begin() + idx, v);
            }
        }
    }

    inline void delete_edge(VertexID u, VertexID v) {
        auto it = edge_map.find(u);
        if (it == edge_map.end()) return; // 没找到起点 u，直接返回

        auto& neighbors = it->second;
        ui idx = b_search::lower_bound_idx(neighbors, v);
        
        // 找到了目标边，执行删除
        if (idx < neighbors.size() && neighbors[idx] == v) {
            neighbors.erase(neighbors.begin() + idx);
        }
    }

    // 全量排序清理函数
    inline void sort_all_edges() {
        for (auto& pair : edge_map) {
            std::sort(pair.second.begin(), pair.second.end());
            // 去重
            pair.second.erase(std::unique(pair.second.begin(), pair.second.end()), pair.second.end());
        }
    }

    vector<VertexID>& operator[] (ui v_idx) { return edge_map[v_idx]; }
    ui v_cnt() const { return edge_map.size(); }
};

#endif //CSM_TYPES_H