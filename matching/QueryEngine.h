#ifndef SUBGRAPHMATCHING_QUERY_ENGINE_H
#define SUBGRAPHMATCHING_QUERY_ENGINE_H

#include "utils/bsx/bsx.h"
#include <vector>
#include <queue>
#include <unordered_set>
#include <bitset>
#include <gmp.h>

class QueryEngine {
public:
    static void
    BSXEngine(ui d_num, const Graph *query_graph, Edges ***edge_matrix, ui **candidates,
              ui *candidates_count, ui *order, size_t output_limit_num,
              mpz_t embedding_cnt, int64_t& time_limit);

private:
    static void bsxMaxCoverOrder(const Graph *graph, ui*& order, ui& num_cover, ui *candidates_count);

    static void bsxDeRefine(BSXIndex& index);

    static VertexID bsxGenNxtU(BSXIndex& index, VertexID* order, ui depth, ui num_cover);

    static bool bsxCheckTermination(ui num, VertexID* indep, std::stack<ui>*valid_cnt);

    static bool bsxGenIndepValidCans(ui indep_num, const VertexID* indep, BSXIndex& index, std::vector<std::vector<VertexID>>& cans);

    static ui bsxRefine(BSXIndex& index, VertexID u);

    static void bsxComEqBatch(BSXIndex& index, VertexID u);

    static ui bsxSepDiff(std::vector<VertexID> &v_cans, const ui *indep_con_cnt, int forward_idx, int backward_idx);

    static void bsxEnumerate4Parts(ui **&sep_flags, const VertexID* nodes, ui num_nodes, std::vector<std::vector<VertexID>>& cans, bool *&visited_v, mpz_t cur_cnt);

    static void bsxComEqBatchDirect(BSXIndex& index, VertexID u, std::vector<ui>& idxs);

    static void bsxGenResult(ui indep_num, const VertexID* indep, BSXIndex& index);

};


#endif //SUBGRAPHMATCHING_QUERY_ENGINE_H
