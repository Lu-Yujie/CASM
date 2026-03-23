#include "graph.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <chrono>

void Graph::BuildReverseIndex() {
    label2v_ = new ui[vertices_count_];
    label2v_offsets_= new ui[vlabels_count_ + 1];
    label2v_offsets_[0] = 0;

    ui total = 0;
    for (ui i = 0; i < vlabels_count_; ++i) {
        label2v_offsets_[i + 1] = total;
        total += vlabels_frequency_[i];
    }

    for (ui i = 0; i < vertices_count_; ++i) {
        LabelID vlabel = vlabels_[i];
        label2v_[label2v_offsets_[vlabel + 1]++] = i;
    }
}

void Graph::BuildVLabelOffset() {
    size_t vlabels_offset_size = (size_t)vertices_count_ * vlabels_count_ + 1;
    vlabels_offsets_ = new ui[vlabels_offset_size];
    std::fill(vlabels_offsets_, vlabels_offsets_ + vlabels_offset_size, 0);

    for (ui i = 0; i < vertices_count_; ++i) {
        std::sort(neighbors_ + offsets_[i], neighbors_ + offsets_[i + 1],
            [this](const VertexID u, const VertexID v) -> bool {
                return vlabels_[u] == vlabels_[v] ? u < v : vlabels_[u] < vlabels_[v];
            });
    }

    for (ui i = 0; i < vertices_count_; ++i) {
        LabelID previous_vlabel = 0;
        LabelID current_vlabel = 0;

        vlabels_offset_size = i * vlabels_count_;
        vlabels_offsets_[vlabels_offset_size] = offsets_[i];

        for (ui j = offsets_[i]; j < offsets_[i + 1]; ++j) {
            current_vlabel = vlabels_[neighbors_[j]];

            if (current_vlabel != previous_vlabel) {
                for (ui k = previous_vlabel + 1; k <= current_vlabel; ++k) {
                    vlabels_offsets_[vlabels_offset_size + k] = j;
                }
                previous_vlabel = current_vlabel;
            }
        }

        for (ui l = current_vlabel + 1; l <= vlabels_count_; ++l) {
            vlabels_offsets_[vlabels_offset_size + l] = offsets_[i + 1];
        }
    }
}

// 构建全局只读的 DataEdgeIndex[Label_A][Label_B]
void Graph::BuildDataEdgeIndex() {
    data_edge_index_.resize(vlabels_count_);
    for (ui i = 0; i < vlabels_count_; ++i) {
        data_edge_index_[i].assign(vlabels_count_, nullptr);
    }

    for (ui u = 0; u < vertices_count_; ++u) {
        LabelID u_label = vlabels_[u];
        
        for (ui j = offsets_[u]; j < offsets_[u + 1]; ++j) {
            VertexID nbr = neighbors_[j];
            LabelID nbr_label = vlabels_[nbr];
            if (data_edge_index_[u_label][nbr_label] == nullptr) {
                data_edge_index_[u_label][nbr_label] = new CSMEdges();
            }

            // load时已经排序，add_edge 优化后按序插入不会做搜索
            data_edge_index_[u_label][nbr_label]->add_edge(u, nbr);
        }
    }

    for (ui i = 0; i < vlabels_count_; ++i) {
        for (ui j = 0; j < vlabels_count_; ++j) {
            if (data_edge_index_[i][j] != nullptr) {
                data_edge_index_[i][j]->sort_all_edges();
            }
        }
    }
}

void Graph::loadGraphFromFile(const std::string &file_path) {
    std::ifstream infile(file_path);

    if (!infile.is_open()) {
        std::cout << "Can not open the graph file " << file_path << " ." << std::endl;
        exit(-1);
    }

    char type;
    infile >> type >> vertices_count_ >> edges_count_;
    offsets_ = new ui[vertices_count_ +  1];
    offsets_[0] = 0;

    neighbors_ = new VertexID[edges_count_ * 2];
    vlabels_ = new LabelID[vertices_count_];
    vlabels_count_ = 0;
    max_degree_ = 0;

    LabelID max_vlabel_id = 0;
    std::vector<ui> neighbors_offset(vertices_count_, 0);

    while (infile >> type) {
        if (type == 'v') { // Read vertex.
            VertexID id;
            LabelID  vlabel;
            ui degree;
            infile >> id >> vlabel >> degree;

            vlabels_[id] = vlabel;
            offsets_[id + 1] = offsets_[id] + degree;

            if (degree > max_degree_) {
                max_degree_ = degree;
            }

            if (vlabels_frequency_.find(vlabel) == vlabels_frequency_.end()) {
                vlabels_frequency_[vlabel] = 0;
                if (vlabel > max_vlabel_id)
                    max_vlabel_id = vlabel;
            }

            vlabels_frequency_[vlabel] += 1;
        }
        else if (type == 'e') { // Read edge.
            VertexID begin;
            VertexID end;
            infile >> begin >> end;

            ui begin_offset = offsets_[begin] + neighbors_offset[begin];
            neighbors_[begin_offset] = end;

            ui end_offset = offsets_[end] + neighbors_offset[end];
            neighbors_[end_offset] = begin;

            neighbors_offset[begin] += 1;
            neighbors_offset[end] += 1;
        }
    }

    infile.close();
    vlabels_count_ = (ui)vlabels_frequency_.size() > (max_vlabel_id + 1) ? (ui)vlabels_frequency_.size() : max_vlabel_id + 1;
    for (auto element : vlabels_frequency_) {
        if (element.second > max_vlabel_frequency_) {
            max_vlabel_frequency_ = element.second;
        }
    }

    for (ui i = 0; i < vertices_count_; ++i) {
        std::sort(neighbors_ + offsets_[i], neighbors_ + offsets_[i + 1]);
    }

    BuildReverseIndex();
    BuildDataEdgeIndex();
}

void Graph::load_updates(const std::string& file_path, std::vector<Update>& stream) {
    uint32_t vertex_num = this->getVerticesCount();
    spp::sparse_hash_map<uint32_t, uint32_t> new_vertex_label;
    Update update;

    std::ifstream ifs(file_path);
    if (!ifs.is_open()) {
        std::cout << "Can not open the stream file " << file_path << " ." << std::endl;
        exit(-1);
    }

    while (ifs.good()) {
        std::string tmp_str;
        std::stringstream ss;
        std::string op_str;
        std::getline(ifs, tmp_str);
        if (tmp_str == "") break;

        if (tmp_str[0] != '#') {
            ss.clear();
            ss << tmp_str;
            ss >> op_str;

            if (op_str == "v") {
                uint32_t id;
                uint32_t label;
                ss >> id >> label;
                if (id < vertex_num) {
                    if (label != this->getVertexLabel(id)) {
                        std::cout << "update label(" << label << ") of " << id <<" is not aligned"
                                  << " with label(" << this->getVertexLabel(id) << ") in data graph" << std::endl;
                        exit(-1);
                    }
                }
                else {
                    new_vertex_label[id] = label;
                }
            } else if (op_str == "e" || op_str == "-e") {
                update.op_ = op_str == "e" ? '+' : '-';
                uint32_t first, second;
                ss >> first >> second;
                update.edge_.src_ = first;
                update.edge_.dst_ = second;
                update.edge_.elabel_.src_label_ = first < vertex_num
                                                  ? this->getVertexLabel(first)
                                                  : new_vertex_label[first];
                update.edge_.elabel_.dst_label_ = second < vertex_num
                                                  ? this->getVertexLabel(second)
                                                  : new_vertex_label[second];

                stream.emplace_back(update);
            } else {
                std::cout << "unsupported op: " << op_str << ", expected #, e, -e" << std::endl;
                exit(-1);
            }
        }
    }

    ifs.close();
}

void Graph::printGraphMetaData() {
    std::cout << "|V|: " << vertices_count_ << ", |E|: " << edges_count_ << ", |\u03A3|: " << vlabels_count_ << std::endl;
    std::cout << "Max Degree: " << max_degree_ << ", Max Label Frequency: " << max_vlabel_frequency_ << std::endl;
}