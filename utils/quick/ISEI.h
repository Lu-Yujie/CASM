#ifndef LU_ISEI_H
#define LU_ISEI_H

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <cstring> 
#include "configuration/types.h"

enum class EngineType {
    QUICK_BASED = 0,
    GROUP_BASED = 1
};

struct CandidateSig {
    uint64_t signature;
    ui original_idx;
    CandidateSig(uint64_t sig, ui idx):signature(sig), original_idx(idx) {}
    CandidateSig():signature(0), original_idx(0) {}
};

class ISEIndex {
private:
    ui vertices_count_;
    ui active_vertices_;          // 记录曾经活跃过的最大点集范围

    uint64_t* R_;           // 每个节点的伪随机标识 (只读)
    uint64_t* Sig_;         // 节点的实时结构签名 (动态异或更新)
    std::mt19937_64 rng_;

    // --- 增量批处理控制 (Delta-Batching) ---
    bool* is_modified_;           // 节点是否在当前批次中被修改过
    uint64_t* old_sig_;           // 节点修改前的签名快照
    VertexID* modified_queue_;    // 当前批次被修改的节点队列
    ui modified_count_;           // 当前批次修改的节点数量
    // 全局等价类表 (Signature -> Set of Vertices)
    std::unordered_map<uint64_t, std::unordered_set<VertexID>> equivalence_classes_;

    ui scale_threshold_;          // Phase 1: 规模门控阈值 N_tau
    double entropy_threshold_;    // Phase 2: 结构熵阈值 tau_ent
    double cached_alpha_ent_;     // 缓存的最新结构熵
    double entropy_sum_;          // 增量状态 S = sum(c_i * log2(c_i))

    // c * log2(c)
    inline double calc_c_log_c(ui c) const {
        return (LIKELY(c > 0)) ? (static_cast<double>(c) * std::log2(static_cast<double>(c))) : 0.0;
    }

    // 阶段更新暂存点
    inline void stageVertex(VertexID node, uint64_t neighbor_R) {
        if (!is_modified_[node]) {
            is_modified_[node] = true;
            old_sig_[node] = Sig_[node];
            modified_queue_[modified_count_++] = node;
        }
        Sig_[node] ^= neighbor_R;
    }

    // O(1) 获取全图结构熵
    inline double getStructureEntropy() const {
        if (UNLIKELY(active_vertices_ <= 1)) return 0.0;

        double N = static_cast<double>(active_vertices_);
        double denominator = N * std::log2(N);
        if (denominator == 0.0) return 0.0;

        return 1.0 - (entropy_sum_ / denominator);
    }

    // 预分组：equivalence_classes_
    void buildInitialIndex(const ui* offsets, const VertexID* neighbors) {
        for (ui u = 0; u < vertices_count_; ++u) {
            uint64_t signature = 0;
            for (ui j = offsets[u]; j < offsets[u + 1]; ++j) {
                VertexID v = neighbors[j];
                signature ^= R_[v];
            }
            Sig_[u] = signature;
            equivalence_classes_[signature].insert(u);
        }

        active_vertices_ = vertices_count_;
        modified_count_ = 0;

        // 初始化增量熵状态 S
        for (const auto& pair : equivalence_classes_) {
            entropy_sum_ += calc_c_log_c(pair.second.size());
        }

        // 预热熵计算
        cached_alpha_ent_ = getStructureEntropy();
    }

    // 翻倍扩容
    inline void ensureCapacity(VertexID max_id) {
        ui new_capacity = vertices_count_;
        while (new_capacity <= max_id) new_capacity *= 2;

        uint64_t* new_R = new uint64_t[new_capacity];
        uint64_t* new_Sig = new uint64_t[new_capacity];
        bool* new_is_mod = new bool[new_capacity]();
        uint64_t* new_old_sig = new uint64_t[new_capacity]();
        VertexID* new_mod_queue = new VertexID[new_capacity];

        // 拷贝旧数据
        std::memcpy(new_R, R_, vertices_count_ * sizeof(uint64_t));
        std::memcpy(new_Sig, Sig_, vertices_count_ * sizeof(uint64_t));
        std::memcpy(new_is_mod, is_modified_, vertices_count_ * sizeof(bool));
        std::memcpy(new_old_sig, old_sig_, vertices_count_ * sizeof(uint64_t));
        std::memcpy(new_mod_queue, modified_queue_, modified_count_ * sizeof(VertexID));

        // 清理扩展区域
        ui extension_size = new_capacity - vertices_count_;
        std::memset(new_Sig + vertices_count_, 0, extension_size * sizeof(uint64_t));

        // 为新节点生成随机标识
        for (ui i = vertices_count_; i < new_capacity; ++i) {
            new_R[i] = rng_();
        }

        // 释放旧内存
        delete[] R_;
        delete[] Sig_;
        delete[] is_modified_;
        delete[] old_sig_;
        delete[] modified_queue_;

        R_ = new_R;
        Sig_ = new_Sig;
        is_modified_ = new_is_mod;
        old_sig_ = new_old_sig;
        modified_queue_ = new_mod_queue;
        vertices_count_ = new_capacity;
    }

    // batch-增量维护等价类与熵值
    void commitBatch() {
        if (UNLIKELY(modified_count_ == 0)) return;

        for (ui i = 0; i < modified_count_; ++i) {
            VertexID u = modified_queue_[i];
            uint64_t old_s = old_sig_[u];
            uint64_t new_s = Sig_[u];

            if (old_s != new_s) {
                // 1. 处理旧等价类擦除
                if (equivalence_classes_.find(old_s) != equivalence_classes_.end()) {
                    auto& old_group = equivalence_classes_[old_s];

                    entropy_sum_ -= calc_c_log_c(old_group.size());
                    old_group.erase(u);
                    entropy_sum_ += calc_c_log_c(old_group.size());

                    if (old_group.empty()) {
                        equivalence_classes_.erase(old_s);
                    }
                }

                // 2. 加入新等价类
                auto& new_group = equivalence_classes_[new_s]; 

                entropy_sum_ -= calc_c_log_c(new_group.size());
                new_group.insert(u);
                entropy_sum_ += calc_c_log_c(new_group.size());
            }

            is_modified_[u] = false;
        }
        modified_count_ = 0;
    }

public:
    ISEIndex() {
        vertices_count_ = 0;
        active_vertices_ = 0;
        R_ = nullptr;
        Sig_ = nullptr;
        is_modified_ = nullptr;
        old_sig_ = nullptr;
        modified_queue_ = nullptr;
        modified_count_ = 0;

        scale_threshold_ = 1000000;
        entropy_threshold_ = 0.6;
        cached_alpha_ent_ = 0.0;
        entropy_sum_ = 0.0;
    }

    ~ISEIndex() {
        delete[] R_;
        delete[] Sig_;
        delete[] is_modified_;
        delete[] old_sig_;
        delete[] modified_queue_;
    }

    // gets
    inline uint64_t getSignature(VertexID v) const { return Sig_[v]; }
    const std::unordered_map<uint64_t, std::unordered_set<VertexID>>& getEquivalenceClasses() const {
        return equivalence_classes_;
    }

    void init(ui initial_capacity, const ui* offsets, const VertexID* neighbors,
              ui N_tau = 1000000, double tau_ent = 0.6, int random_seed = 1337){
        vertices_count_ = initial_capacity;
        if (vertices_count_ == 0) vertices_count_ = 1;

        scale_threshold_ = N_tau;
        entropy_threshold_ = tau_ent;

        R_ = new uint64_t[vertices_count_];
        Sig_ = new uint64_t[vertices_count_](); // 初始化为 0
        is_modified_ = new bool[vertices_count_](); 
        old_sig_ = new uint64_t[vertices_count_]();
        modified_queue_ = new VertexID[vertices_count_];
        rng_.seed(random_seed);

        for (ui i = 0; i < vertices_count_; ++i) {
            R_[i] = rng_();
        }
        buildInitialIndex(offsets, neighbors);
    }

    // 更新
    inline void stageEdgeUpdate(VertexID u, VertexID v) {
        VertexID max_id = (u > v) ? u : v;
        if (UNLIKELY(max_id >= vertices_count_)) {
            ensureCapacity(max_id);
        }

        if (max_id >= active_vertices_) {
            active_vertices_ = max_id + 1;
        }

        stageVertex(u, R_[v]);
        stageVertex(v, R_[u]);
    }

    // (Decision Function D(G))
    EngineType routeQuery() {
        // Phase 1: 规模门控 (Scale Gating)
        if (active_vertices_ < scale_threshold_) {
            return EngineType::QUICK_BASED; 
        }

        // Phase 2: 延迟批处理提交 & 熵判定
        if (modified_count_ > 0) {
            commitBatch(); 
        }

        // O(1) 获取最新结构熵
        cached_alpha_ent_ = getStructureEntropy();

        // 阈值判定
        if (cached_alpha_ent_ > entropy_threshold_) {
            return EngineType::GROUP_BASED; 
        } else {
            return EngineType::QUICK_BASED;    
        }
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
