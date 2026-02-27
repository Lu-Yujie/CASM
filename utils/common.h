#ifndef LU_COMMON_H
#define LU_COMMON_H

#include "graph/graph.h"
using namespace std;

/**
 * Embedding info
 * mapping between depth, u(query node), v(data node)
 * u2v: u->v, each u only match to one v
 * v2depth: v->depth, too much v, use map instead of array
 * depth2u: depth->u, use vector for dynamic tree height
 */
class Embedding {
public:
  VertexID *u2v;            // u->v, use the 1st v of batch
  vector<VertexID> depth2u; // depth->u

  Embedding(ui cnt) {
    u2v = new VertexID[cnt];
    depth2u.reserve(cnt);
  }
  ~Embedding() {
    delete[] u2v;
  }
};

#endif
