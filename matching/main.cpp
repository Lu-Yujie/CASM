#include <chrono>
#include <fstream>

#include "graph/graph.h"
#include "timeOp.h"
#include "utils/CLI11.hpp"
#include "CSMEngine.h"

#define NANOSECTOSEC(elapsed_time) ((elapsed_time)/(double)1000000000)
#define BYTESTOMB(memory_cost) ((memory_cost)/(double)(1024 * 1024))

int main(int argc, char** argv) {
    CLI::App app{"App description"};
    std::string query_file, data_file, stream_file, max_embedding_num, output_file;
    int64_t time_limit; // 1000ms by default

    app.add_option("-q,--query", query_file, "query graph file")->required();
    app.add_option("-d,--data", data_file, "initial data graph file")->required();
    app.add_option("-u,--update", stream_file, "data graph update stream file")->required();
    app.add_option("-t,--time_limit", time_limit, "time limit(millisecond)")->default_val(300000);  // 300s
    app.add_option("-n,--num", max_embedding_num, "max embedding number")->default_val("MAX");
    app.add_option("-o,--output_file", output_file, "output file")->default_val("./output.csv");

    CLI11_PARSE(app, argc, argv);

    /**
     * Output the command line information.
     */
    std::cout << "Command Line:" << std::endl;
    std::cout << "\tData Graph: " << data_file << std::endl;
    std::cout << "\tQuery Graph: " << query_file << std::endl;
    std::cout << "\tUpdate File: " << stream_file << std::endl;
    std::cout << "\tOutput Limit: " << max_embedding_num << std::endl;
    std::cout << "\tTime Limit (millisecond): " << time_limit << std::endl;
    std::cout << "--------------------------------------------------------------------" << std::endl;

    /**
     * Load input graphs.
     */
    std::cout << "Load graphs..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    Graph* query_graph = new Graph();
    query_graph->loadGraphFromFile(query_file);
    query_graph->g_name = query_file;

    Graph* data_graph = new Graph();
    data_graph->loadGraphFromFile(data_file);

    std::vector<Update> updates;
    data_graph->load_updates(stream_file, updates);
    auto update_cnt = updates.size();

    auto end = std::chrono::high_resolution_clock::now();
    double load_graphs_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << "----- Query Graph Meta" << std::endl;
    query_graph->printGraphMetaData();
    std::cout << "----- Data Graph Meta" << std::endl;
    data_graph->printGraphMetaData();
    std::cout << "----- # Updates:" << update_cnt;
    std::cout << "--------------------------------------------------------------------" << std::endl;

    /**
     * Start queries.
     */
    std::cout << "Start queries..." << std::endl;
    /**
     * init variables, set limits
     */
    vector<mpz_t> embedding_cnts(update_cnt);
    for (auto& embedding_cnt :embedding_cnts) mpz_init_set_ui(embedding_cnt, 0);
    size_t output_limit = 0;
    if (max_embedding_num == "MAX") {
        output_limit = numeric_limits<uint64_t>::max();
    } else {
        sscanf(max_embedding_num.c_str(), "%zu", &output_limit);
    }
    auto end_time = TimeOp::getClockNan();
    end_time += time_limit * 1000 * 1000;

    std::cout << "Build auxiliary structure..." << std::endl;
    start = std::chrono::high_resolution_clock::now();

    CSMEngine* csmEngine = new CSMEngine;
    csmEngine->init(data_graph, query_graph);

    end = std::chrono::high_resolution_clock::now();
    double preprocessing_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << "Scan streams..." << std::endl;
    QueryStats stats;
    start = std::chrono::high_resolution_clock::now();

    ui processed_edges_cnt = 0;
    for (; processed_edges_cnt < update_cnt; processed_edges_cnt++) {
        auto& update_edge = updates[processed_edges_cnt];
        auto& embedding_cnt = embedding_cnts[processed_edges_cnt];
        csmEngine->query(update_edge, output_limit, embedding_cnt, end_time, stats);
        // gmp_printf("#%d Embeddings: %Zd, ", processed_edges_cnt, embedding_cnt);
        if (TimeOp::getClockNan() >= end_time) break;
    }

    end = std::chrono::high_resolution_clock::now();
    double enumeration_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    stats.print_report();

    delete csmEngine;
    delete query_graph;
    delete data_graph;

    /**
     * End.
     */
    std::cout << "--------------------------------------------------------------------" << std::endl;
    double total_time_in_ns = preprocessing_time_in_ns + enumeration_time_in_ns;
    mpz_t total_embeddings;
    mpz_init_set_ui(total_embeddings, 0);
    for (auto& embedding_cnt : embedding_cnts) mpz_add(total_embeddings, total_embeddings, embedding_cnt);
    std::cout << "Load graphs time (seconds): " << NANOSECTOSEC(load_graphs_time_in_ns) << std::endl;
    std::cout << "Preprocessing time (seconds): " << NANOSECTOSEC(preprocessing_time_in_ns) << std::endl;
    std::cout << "Enumerate time (seconds): " << NANOSECTOSEC(enumeration_time_in_ns) << std::endl;
    std::cout << "Total time (seconds): " << NANOSECTOSEC(total_time_in_ns) << std::endl;
    std::cout << "processed edges: " << processed_edges_cnt << "/" << update_cnt << std::endl;
    gmp_printf("#Total Embeddings: %Zd ", total_embeddings);
    std::cout << "\nEnd." << std::endl;

    /**
     * Set the output stream and record the command line information
     */
    std::fstream output;
    char* cnt = new char[2048];
    output.open(output_file, std::ios::out | std::ios::app);
    output << query_file;
    output << "," << data_file;
    output << "," << NANOSECTOSEC(load_graphs_time_in_ns);
    output << "," << NANOSECTOSEC(preprocessing_time_in_ns);
    output << "," << NANOSECTOSEC(enumeration_time_in_ns);
    output << "," << NANOSECTOSEC(total_time_in_ns);
    output << "," << processed_edges_cnt << "/" << update_cnt;
    for (auto& embedding_cnt : embedding_cnts) {
        mpz_get_str(cnt, 10, embedding_cnt);
        output << "," << cnt;
    }
    mpz_get_str(cnt, 10, total_embeddings);
    output << "," << cnt;
    output << std::endl;
    output.close();
    delete[] cnt;
    for (auto& embedding_cnt :embedding_cnts) mpz_clear(embedding_cnt);
    mpz_clear(total_embeddings);

    return 0;
}