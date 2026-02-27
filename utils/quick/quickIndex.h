#ifndef LU_PARTIAL_H
#define LU_PARTIAL_H

/**
 * define data structure used in partial query
 */
#include <stack>
#include <unordered_map>
#include <bitset>
#include <gmp.h>
#include "pretty_print.h"
#include "utils/setOp.h"
#include "utils/bSearch.h"
#include "utils/timeOp.h"
#include "utils/common.h"
#include "quickIndep.h"
#include "validCans.h"
using namespace std;
typedef unsigned int ui;

/**
 * new index structure, update in time
 */
class QuickIndex {
public:
  bool *visited_u;
  bool *visited_v;
  ui *valid_idx; // #valid_idx_

  VertexID** influenced_u; // influenced nodes of each u
  ui* influenced_u_cnt;
  Embedding *embedding;
  QuickIndep *indep_info;
  vector<ValidCans> valid_cans;
  ui qnum;

  QuickIndex(ui dnum, ui max_cans, const Graph* query_graph) {
    auto& qnum = query_graph->getVerticesCount();
    this->qnum = qnum; 
    visited_u = new bool[qnum];
    memset(visited_u, false, sizeof(bool) * qnum);
    visited_v = new bool[dnum];
    memset(visited_v, false, sizeof(bool) * dnum);
    valid_idx = new ui[qnum];
    influenced_u_cnt = new ui[qnum];
    influenced_u = new VertexID*[qnum];
    for (int i = 0; i < qnum; ++i) {
      influenced_u[i] = new VertexID[qnum];
    }

    embedding = new Embedding(qnum);
    indep_info = new QuickIndep(dnum, query_graph, visited_u);
    memset(visited_u, false, sizeof(bool) * qnum);
    valid_cans.reserve(qnum);
    for (int i = 0; i < qnum; ++i) {
      valid_cans.emplace_back(max_cans, qnum);
    }
  }

  ~QuickIndex() {
    delete[] visited_u;
    delete[] visited_v;
    delete[] valid_idx;
    delete[] influenced_u_cnt;
    for (ui i = 0; i < qnum; i++) delete[] influenced_u[i];
    delete[]influenced_u;
    delete embedding;
    delete indep_info;
  }
};

#endif
