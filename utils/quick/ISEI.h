#ifndef LU_ISEI_H
#define LU_ISEI_H

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <cstring> 
#include "configuration/types.h"

struct CandidateSig {
    uint64_t signature;
    ui original_idx;
    CandidateSig(uint64_t sig, ui idx):signature(sig), original_idx(idx) {}
    CandidateSig():signature(0), original_idx(0) {}
};

class ISEIndex {
private:
    ui vertices_count_;     

    uint64_t* R_;           // 每个节点的伪随机标识 (只读)
    uint64_t* Sig_;         // 节点的实时结构签名 (动态异或更新)
    std::mt19937_64 rng_;

public:
    ISEIndex() {
        vertices_count_ = 0;
        R_ = nullptr;
        Sig_ = nullptr;
    }

    void init(ui initial_capacity, int random_seed = 1337){
        vertices_count_ = initial_capacity;
        if (vertices_count_ == 0) vertices_count_ = 1;

        R_ = new uint64_t[vertices_count_];
        Sig_ = new uint64_t[vertices_count_](); // 初始化为 0
        rng_.seed(random_seed);

        for (ui i = 0; i < vertices_count_; ++i) {
            R_[i] = rng_();
        }
    }

    ~ISEIndex() {
        delete[] R_;
        delete[] Sig_;
    }

    inline uint64_t getSignature(VertexID v) const { return Sig_[v]; }

    // 1. 翻倍扩容
    inline void ensureCapacity(VertexID max_id) {
        if (max_id < vertices_count_) return;

        ui new_capacity = vertices_count_;
        while (new_capacity <= max_id) new_capacity *= 2;

        uint64_t* new_R = new uint64_t[new_capacity];
        uint64_t* new_Sig = new uint64_t[new_capacity];

        std::memcpy(new_R, R_, vertices_count_ * sizeof(uint64_t));
        std::memcpy(new_Sig, Sig_, vertices_count_ * sizeof(uint64_t));

        ui extension_size = new_capacity - vertices_count_;
        std::memset(new_Sig + vertices_count_, 0, extension_size * sizeof(uint64_t));

        for (ui i = vertices_count_; i < new_capacity; ++i) {
            new_R[i] = rng_();
        }

        delete[] R_;
        delete[] Sig_;

        R_ = new_R;
        Sig_ = new_Sig;
        vertices_count_ = new_capacity;
    }

    // 更新
    inline void updateEdge(VertexID u, VertexID v) {
        VertexID max_id = (u > v) ? u : v;
        if (max_id >= vertices_count_) {
            ensureCapacity(max_id);
        }

        Sig_[u] ^= R_[v];
        Sig_[v] ^= R_[u];
    }

    // 预分组 (On-the-fly Grouping)
    const std::vector<CandidateSig>& getPreGroup(const VertexID* candidates, ui num_candidates) const {
        // 使用 thread_local 避免运行时的内存分配开销
        thread_local std::vector<CandidateSig> sig_pairs;
        sig_pairs.resize(num_candidates);

        // 提取特征值，并排序
        for (ui i = 0; i < num_candidates; ++i) {
            VertexID v = candidates[i];
            sig_pairs[i] = {Sig_[v], i};
        }
        std::sort(sig_pairs.begin(), sig_pairs.end(), [](const CandidateSig& a, const CandidateSig& b) {
            return a.signature < b.signature;
        });

        return sig_pairs;
    }
};

#endif  // LU_ISEI_H
