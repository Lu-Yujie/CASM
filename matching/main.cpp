#include <chrono>
#include <fstream>
#include <filesystem>
#include <limits>
#include <vector>
#include <iostream>

#include "graph/graph.h"
#include "timeOp.h"
#include "utils/CLI11.hpp"
#include "CSMEngine.h"

#define NANOSECTOSEC(elapsed_time) ((elapsed_time)/(double)1000000000)
#define BYTESTOMB(memory_cost) ((memory_cost)/(double)(1024 * 1024))

int main(int argc, char** argv) {
    CLI::App app{"App description"};
    std::string query_path, data_file, stream_file, max_embedding_num, output_file;
    int64_t time_limit;

    app.add_option("-q,--query", query_path, "query graph file or directory")->required();
    app.add_option("-d,--data", data_file, "initial data graph file")->required();
    app.add_option("-u,--update", stream_file, "data graph update stream file")->required();
    app.add_option("-t,--time_limit", time_limit, "time limit(millisecond)")->default_val(300000);  // 300s
    app.add_option("-n,--num", max_embedding_num, "max embedding number")->default_val("MAX");
    app.add_option("-o,--output_file", output_file, "output file")->default_val("./output.csv");

    CLI11_PARSE(app, argc, argv);

    std::cout << "Loading Data Graph & Updates (Global)..." << std::endl;
    auto data_start = std::chrono::high_resolution_clock::now();
    Graph* data_graph = new Graph();
    data_graph->loadGraphFromFile(data_file);

    std::vector<Update> updates;
    data_graph->load_updates(stream_file, updates);
    auto update_cnt = updates.size();
    auto data_end = std::chrono::high_resolution_clock::now();
    double data_load_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(data_end - data_start).count();

    std::cout << "----- Data Graph Meta" << std::endl;
    data_graph->printGraphMetaData();
    std::cout << "----- # Updates:" << update_cnt << std::endl;

    size_t output_limit = 0;
    if (max_embedding_num == "MAX") {
        output_limit = std::numeric_limits<uint64_t>::max();
    } else {
        sscanf(max_embedding_num.c_str(), "%zu", &output_limit);
    }

    char* cnt_str = new char[2048];
    mpz_t embedding_cnt;
    mpz_init(embedding_cnt);
    mpz_t total_embeddings;
    mpz_init(total_embeddings);

    // 2. prepare query file list
    std::vector<std::string> query_files;
    if (std::filesystem::is_regular_file(query_path)) {
        query_files.push_back(query_path);
    } else if (std::filesystem::is_directory(query_path)) {
        for (const auto& entry : std::filesystem::directory_iterator(query_path)) {
            if (entry.is_regular_file()) query_files.push_back(entry.path().string());
        }
    } else {
        std::cerr << "Error: Invalid query path!" << std::endl;
        return -1;
    }

    for (const auto& query_file : query_files) {
        std::cout << "--------------------------------------------------------------------" << std::endl;
        std::cout << "Load query graph:" << query_file << std::endl;
        // 紧贴核心操作计时
        auto query_start = std::chrono::high_resolution_clock::now();
        Graph* query_graph = new Graph();
        query_graph->loadGraphFromFile(query_file);
        query_graph->g_name = query_file;
        auto query_end = std::chrono::high_resolution_clock::now();

        double load_graphs_time_in_ns = data_load_time_in_ns + std::chrono::duration_cast<std::chrono::nanoseconds>(query_end - query_start).count();

        std::cout << "----- Query Graph Meta" << std::endl;
        query_graph->printGraphMetaData();

        auto end_time = TimeOp::getClockNan();
        end_time += time_limit * 1000 * 1000;

        std::cout << "Build auxiliary structure..." << std::endl;
        auto start_pre = std::chrono::high_resolution_clock::now();
        CSMEngine* csmEngine = new CSMEngine;
        csmEngine->init(data_graph, query_graph);
        auto end_pre = std::chrono::high_resolution_clock::now();
        double preprocessing_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_pre - start_pre).count();

        std::cout << "Scan streams..." << std::endl;
        QueryStats stats;

        // 紧贴 Enumeration 计时
        auto start_enum = std::chrono::high_resolution_clock::now();
        ui processed_edges_cnt = 0;
        mpz_set_ui(total_embeddings, 0);
        for (; processed_edges_cnt < update_cnt; processed_edges_cnt++) {
            auto& update_edge = updates[processed_edges_cnt];
            mpz_set_ui(embedding_cnt, 0);
            csmEngine->query(update_edge, output_limit, embedding_cnt, end_time, stats);
            mpz_add(total_embeddings, total_embeddings, embedding_cnt);
            if (TimeOp::getClockNan() >= end_time) break;
        }
        auto end_enum = std::chrono::high_resolution_clock::now();
        double enumeration_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_enum - start_enum).count();

        stats.print_report();

        double total_time_in_ns = preprocessing_time_in_ns + enumeration_time_in_ns;

        // 打印总结（不在计时范围内）
        std::cout << "Load graphs time (seconds): " << NANOSECTOSEC(load_graphs_time_in_ns) << std::endl;
        std::cout << "Preprocessing time (seconds): " << NANOSECTOSEC(preprocessing_time_in_ns) << std::endl;
        std::cout << "Enumerate time (seconds): " << NANOSECTOSEC(enumeration_time_in_ns) << std::endl;
        std::cout << "Total time (seconds): " << NANOSECTOSEC(total_time_in_ns) << std::endl;
        gmp_printf("#Total Embeddings: %Zd\n", total_embeddings);

        // 3. 将结果输出到 CSV 文件
        std::fstream output;
        output.open(output_file, std::ios::out | std::ios::app);
        output << query_file << "," << data_file << ","
               << NANOSECTOSEC(load_graphs_time_in_ns) << ","
               << NANOSECTOSEC(preprocessing_time_in_ns) << ","
               << NANOSECTOSEC(enumeration_time_in_ns) << ","
               << NANOSECTOSEC(total_time_in_ns) << ","
               << processed_edges_cnt << "/" << update_cnt;
        mpz_get_str(cnt_str, 10, total_embeddings);
        output << "," << cnt_str << std::endl;
        output.close();

        // 4. 清理当前 Query 的内存
        delete csmEngine;
        delete query_graph;
    }

    // 全局清理
    mpz_clear(embedding_cnt);
    mpz_clear(total_embeddings);
    delete[] cnt_str;
    delete data_graph;

    return 0;
}