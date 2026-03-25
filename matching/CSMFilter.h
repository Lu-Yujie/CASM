#ifndef CSM_FILTER_H
#define CSM_FILTER_H

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include "graph/graph.h"

class CSMFilter {
public:
    std::vector<ui> degree;
    std::vector<ui> labels;

    CSMFilter() = default;
    ~CSMFilter() = default;

    void init(const Graph* data_graph) {
        ui capacity = data_graph->getVerticesCount();
        if (capacity == 0) capacity = 1024;
        degree.assign(capacity, 0);
        labels.assign(capacity, 0);

        ui* deg_ptr = degree.data();
        ui* lab_ptr = labels.data();
        for (ui i = 0; i < data_graph->getVerticesCount(); ++i) {
            deg_ptr[i] = data_graph->getVertexDegree(i);
            lab_ptr[i] = data_graph->getVertexLabel(i);
        }
    }

    inline void update_filter(Update& de) {
        auto v_src = de.edge_.src();
        auto v_dst = de.edge_.dst();
        char op = de.op_;

        VertexID max_v = std::max(v_src, v_dst);
        if (UNLIKELY(max_v >= degree.size())) {
            ui new_capacity = degree.capacity();
            if (new_capacity == 0) new_capacity = 1024;

            // double capacity
            while (max_v >= new_capacity) {
                new_capacity *= 2;
            }

            degree.resize(new_capacity, 0);
            labels.resize(new_capacity, 0);
            labels[v_src] = de.src_label();
            labels[v_dst] = de.dst_label();
        }

        if (op == '+') {
            degree[v_src]++;
            degree[v_dst]++;
        } else { // op == '-'
            degree[v_src]--;
            degree[v_dst]--;
        }
    }

    inline bool filter_check(VertexID& v, ui& q_degree, ui& q_label) const {
        return labels[v] == q_label && degree[v] >= q_degree;
    }
};

#endif
