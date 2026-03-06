#ifndef SUBGRAPHMATCHING_TYPES_H
#define SUBGRAPHMATCHING_TYPES_H

#include <cstdint>
#include <stdlib.h>
#include <functional>
#include <vector>
#include <absl/container/flat_hash_map.h>

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
    Edge(uint32_t src, uint32_t dst, uint32_t src_label = 0, uint32_t dst_label = 0):
        elabel_(src_label, dst_label) {
        src_ = src;
        dst_ = dst;
    }
    Edge() {}
    bool operator==(const Edge& l) const {
        return l.src_ == src_ && l.dst_ == dst_ && l.elabel_ == elabel_;
    }
    uint32_t& src() { return src_; }
    uint32_t& dst() { return dst_; }
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
    vector<VertexID>& operator[] (ui v_idx) { return edge_map[v_idx]; }
    ui v_cnt() { return edge_map.size(); }
};

#endif //SUBGRAPHMATCHING_TYPES_H
