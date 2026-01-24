#ifndef SUBGRAPHMATCHING_TYPES_H
#define SUBGRAPHMATCHING_TYPES_H

#include <cstdint>
#include <stdlib.h>
#include <functional> 

typedef unsigned int ui;

typedef uint32_t VertexID;
typedef ui LabelID;

struct ELabel {
    uint32_t src_label_;
    uint32_t dst_label_;
    bool operator==(const ELabel& l) const {
        return l.src_label_ == src_label_ && l.dst_label_ == dst_label_;
    }
    ELabel(uint32_t src, uint32_t dst): src_label_(src), dst_label_(dst) {}
    ELabel(): src_label_(0), dst_label_(0) {}
};

struct Edge {
    uint32_t vertices_[2];
    ELabel elabel_;
    Edge(uint32_t src, uint32_t dst, uint32_t src_label = 0, uint32_t dst_label = 0):
        elabel_(src_label, dst_label) {
        vertices_[0] = src;
        vertices_[1] = dst;
    }
    Edge() {}
    bool operator==(const Edge& l) const {
        return l.vertices_[0] == vertices_[0] && l.vertices_[1] == vertices_[1] && l.elabel_ == elabel_;
    }
    uint32_t operator[](ui index) { return vertices_[index]; }
    uint32_t& src() { return vertices_[0]; }
    uint32_t& dst() { return vertices_[1]; }
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
            size_t h1 = std::hash<uint32_t>()(k.vertices_[0]);
            size_t h2 = std::hash<uint32_t>()(k.vertices_[1]);
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

class TreeNode {
public:
    VertexID id_;
    VertexID parent_;
    ui level_;
    ui under_level_count_;
    ui children_count_;
    ui bn_count_;
    ui fn_count_;
    VertexID* under_level_;
    VertexID* children_;
    VertexID* bn_;
    VertexID* fn_;
    size_t estimated_embeddings_num_;
public:
    TreeNode() {
        id_ = 0;
        under_level_ = nullptr;
        bn_ = nullptr;
        fn_ = nullptr;
        children_ = nullptr;
        parent_ = 0;
        level_ = 0;
        under_level_count_ = 0;
        children_count_ = 0;
        bn_count_ = 0;
        fn_count_ = 0;
        estimated_embeddings_num_ = 0;
    }

    ~TreeNode() {
        delete[] under_level_;
        delete[] bn_;
        delete[] fn_;
        delete[] children_;
    }

    void initialize(const ui size) {
        under_level_ = new VertexID[size];
        bn_ = new VertexID[size];
        fn_ = new VertexID[size];
        children_ = new VertexID[size];
    }
};

class Edges {
public:
    ui* offset_;
    ui* edge_;
    ui vertex_count_;
    ui edge_count_;
    ui max_degree_;
public:
    Edges() {
        offset_ = nullptr;
        edge_ = nullptr;
        vertex_count_ = 0;
        edge_count_ = 0;
        max_degree_ = 0;
    }

    ~Edges() {
        delete[] offset_;
        delete[] edge_;
    }
};

#endif //SUBGRAPHMATCHING_TYPES_H
