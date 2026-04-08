#ifndef CSM_GRAPH_H
#define CSM_GRAPH_H

#include <unordered_map>
#include <iostream>
#include <vector>
#include "configuration/types.h"
#include "configuration/config.h"

class Graph {
public:
    std::string g_name;
private:
    ui vertices_count_;
    ui edges_count_;
    ui vlabels_count_;
    ui max_degree_;
    ui max_vlabel_frequency_;

    ui* offsets_;
    VertexID * neighbors_;
    LabelID* vlabels_;  // v->label
    ui* label2v_offsets_;
    VertexID* label2v_;  // label->v

    std::unordered_map<LabelID, ui> vlabels_frequency_;

    ui* vlabels_offsets_;

    // 全局只读标签边索引: DataEdgeIndex[Label_A][Label_B], 如果 label 数量超级巨大，请替换成 map 结构
    // 请不要因为这里数据结构替换的问题，说我们代码不支持大量 label 的异质图
    std::vector<std::vector<CSMEdges*>> data_edge_index_;

private:
    void BuildReverseIndex();
    void BuildVLabelOffset();

    // 构建全局标签边索引
    void BuildDataEdgeIndex(); 

public:
    Graph() {
        vertices_count_ = 0;
        edges_count_ = 0;
        vlabels_count_ = 0;
        max_degree_ = 0;
        max_vlabel_frequency_ = 0;

        offsets_ = nullptr;
        neighbors_ = nullptr;
        vlabels_ = nullptr;
        label2v_offsets_ = nullptr;
        label2v_ = nullptr;
        vlabels_frequency_.clear();
        vlabels_offsets_ = nullptr;
    }

    ~Graph() {
        delete[] offsets_;
        delete[] neighbors_;
        delete[] vlabels_;
        delete[] label2v_offsets_;
        delete[] label2v_;
        delete[] vlabels_offsets_;

        for (auto& row : data_edge_index_) {
            for (auto* edge_ptr : row) {
                if (edge_ptr != nullptr) {
                    delete edge_ptr;
                }
            }
        }
    }

public:
    void loadGraphFromFile(const std::string& file_path);
    void load_updates(const std::string& file_path, std::vector<Update>& stream);
    void printGraphMetaData();

public:
    // 只读全局索引查询接口
    inline const CSMEdges* getDataEdgeIndex(LabelID src_label, LabelID dst_label) const {
        if (src_label >= vlabels_count_ || dst_label >= vlabels_count_) {
            return nullptr;
        }
        return data_edge_index_[src_label][dst_label];
    }

    inline const ui& getLabelsCount() const {
        return vlabels_count_;
    }

    inline const ui& getVerticesCount() const {
        return vertices_count_;
    }

    inline const ui& getEdgesCount() const {
        return edges_count_;
    }

    inline const ui& getGraphMaxDegree() const {
        return max_degree_;
    }

    inline const ui& getGraphMaxLabelFrequency() const {
        return max_vlabel_frequency_;
    }

    const ui getVertexDegree(const VertexID id) const {
        return offsets_[id + 1] - offsets_[id];
    }

    const ui getLabelsFrequency(const LabelID label) const {
        return vlabels_frequency_.find(label) == vlabels_frequency_.end() ? 0 : vlabels_frequency_.at(label);
    }

    const LabelID getVertexLabel(const VertexID id) const {
        return vlabels_[id];
    }

    const ui * getVertexNeighbors(const VertexID id, ui& count) const {
        count = offsets_[id + 1] - offsets_[id];
        return neighbors_ + offsets_[id];
    }

    const ui * getVerticesByLabel(const LabelID id, ui& count) const {
        count = label2v_offsets_[id + 1] - label2v_offsets_[id];
        return label2v_ + label2v_offsets_[id];
    }

    const ui * getEdges() const {
        return neighbors_;
    }

    const ui * getOffsets() const {
        return offsets_;
    }

    const ui * getNeighborsByLabel(const VertexID id, const LabelID label, ui& count) const {
        ui offset = id * vlabels_count_ + label;
        count = vlabels_offsets_[offset + 1] - vlabels_offsets_[offset];
        return neighbors_ + vlabels_offsets_[offset];
    }

    bool checkEdgeExistence(const VertexID u, const VertexID v, const LabelID u_label) const {
        ui count = 0;
        const VertexID* neighbors = getNeighborsByLabel(v, u_label, count);
        int begin = 0;
        int end = count - 1;
        while (begin <= end) {
            int mid = begin + ((end - begin) >> 1);
            if (neighbors[mid] == u) {
                return true;
            }
            else if (neighbors[mid] > u)
                end = mid - 1;
            else
                begin = mid + 1;
        }

        return false;
    }

    bool checkEdgeExistence(VertexID u, VertexID v) const {
        if (getVertexDegree(u) < getVertexDegree(v)) {
            std::swap(u, v);
        }
        ui count = 0;
        const VertexID* neighbors =  getVertexNeighbors(v, count);

        int begin = 0;
        int end = count - 1;
        while (begin <= end) {
            int mid = begin + ((end - begin) >> 1);
            if (neighbors[mid] == u) {
                return true;
            }
            else if (neighbors[mid] > u)
                end = mid - 1;
            else
                begin = mid + 1;
        }

        return false;
    }
};

#endif //CSM_GRAPH_H