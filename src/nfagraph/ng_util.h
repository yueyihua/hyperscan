/*
 * Copyright (c) 2015-2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/** \file
 * \brief Miscellaneous NFA graph utilities.
 */
#ifndef NG_UTIL_H
#define NG_UTIL_H

#include <map>
#include <vector>

#include <boost/graph/depth_first_search.hpp> // for default_dfs_visitor

#include "ng_holder.h"
#include "ue2common.h"
#include "util/graph.h"
#include "util/graph_range.h"
#include "util/ue2_containers.h"

namespace ue2 {

struct Grey;
struct NFAVertexDepth;
struct ue2_literal;
class depth;
class ReportManager;

depth maxDistFromInit(const NFAVertexDepth &d);
depth maxDistFromStartOfData(const NFAVertexDepth &d);

/** True if the given vertex is a dot (reachable on any character). */
template<class GraphT>
static really_inline
bool is_dot(NFAVertex v, const GraphT &g) {
    return g[v].char_reach.all();
}

/** adds successors of v to s */
template<class U>
static really_inline
void succ(const NGHolder &g, NFAVertex v, U *s) {
    auto rv = adjacent_vertices(v, g);
    s->insert(rv.first, rv.second);
}

template<class ContTemp = flat_set<NFAVertex>>
ContTemp succs(NFAVertex u, const NGHolder &g) {
    ContTemp rv;
    succ(g, u, &rv);
    return rv;
}

/** adds predecessors of v to s */
template<class U>
static really_inline
void pred(const NGHolder &g, NFAVertex v, U *p) {
    auto rv = inv_adjacent_vertices(v, g);
    p->insert(rv.first, rv.second);
}

template<class ContTemp = flat_set<NFAVertex>>
ContTemp preds(NFAVertex u, const NGHolder &g) {
    ContTemp rv;
    pred(g, u, &rv);
    return rv;
}

/** returns a vertex with an out edge from v and is not v.
 * v must have exactly one out-edge excluding self-loops.
 * will return NGHolder::null_vertex() if the preconditions don't hold.
 */
NFAVertex getSoleDestVertex(const NGHolder &g, NFAVertex v);

/** Like getSoleDestVertex but for in-edges */
NFAVertex getSoleSourceVertex(const NGHolder &g, NFAVertex v);

/** \brief edge filtered graph.
 *
 * This will give you a view over the graph that has none of the edges from
 * the provided set included.
 *
 * If this is provided with the back edges of the graph, this will result in an
 * acyclic subgraph view. This is useful for topological_sort and other
 * algorithms that require a DAG.
 */
template<typename EdgeSet>
struct bad_edge_filter {
    bad_edge_filter() {}
    explicit bad_edge_filter(const EdgeSet *bad_e) : bad_edges(bad_e) {}
    bool operator()(const typename EdgeSet::value_type &e) const {
        return !contains(*bad_edges, e); /* keep edges not in the bad set */
    }
    const EdgeSet *bad_edges = nullptr;
};

template<typename EdgeSet>
bad_edge_filter<EdgeSet> make_bad_edge_filter(const EdgeSet *e) {
    return bad_edge_filter<EdgeSet>(e);
}

/** Visitor that records back edges */
template <typename BackEdgeSet>
class BackEdges : public boost::default_dfs_visitor {
public:
    explicit BackEdges(BackEdgeSet &edges) : backEdges(edges) {}
    template <class EdgeT, class GraphT>
    void back_edge(const EdgeT &e, const GraphT &) {
        backEdges.insert(e); // Remove this back edge only
    }
    BackEdgeSet &backEdges;
};

/** Returns true if the vertex is either of the real starts (NODE_START,
 *  NODE_START_DOTSTAR). */
template <typename GraphT>
static really_inline
bool is_any_start(typename GraphT::vertex_descriptor v, const GraphT &g) {
    u32 i = g[v].index;
    return i == NODE_START || i == NODE_START_DOTSTAR;
}

bool is_virtual_start(NFAVertex v, const NGHolder &g);

template <typename GraphT>
bool is_any_accept(typename GraphT::vertex_descriptor v, const GraphT &g) {
    u32 i = g[v].index;
    return i == NODE_ACCEPT || i == NODE_ACCEPT_EOD;
}

/** returns true iff v has an edge to accept or acceptEod */
template <typename GraphT>
bool is_match_vertex(typename GraphT::vertex_descriptor v, const GraphT &g) {
    return edge(v, g.accept, g).second || edge(v, g.acceptEod, g).second;
}

/** Generate a reverse topological ordering for a back-edge filtered version of
 * our graph (as it must be a DAG and correctly numbered).
 *
 * Note: we ensure that we produce a topo ordering that begins with acceptEod
 * and accept (if present) and ends with startDs followed by start.
 */
std::vector<NFAVertex> getTopoOrdering(const NGHolder &g);

bool onlyOneTop(const NGHolder &g);

/** Return the set of the tops on the given graph. */
flat_set<u32> getTops(const NGHolder &h);

/** Initialise the tops on h to the provide top. Assumes that h is triggered and
 * no tops have been set on h. */
void setTops(NGHolder &h, u32 top = DEFAULT_TOP);

/** adds a vertex to g with all the same vertex properties as \p v (aside from
 * index) */
NFAVertex clone_vertex(NGHolder &g, NFAVertex v);

/**
 * \brief Copies all out-edges from source to target.
 *
 * Edge properties (aside from index) are preserved and duplicate edges are
 * skipped.
 */
void clone_out_edges(NGHolder &g, NFAVertex source, NFAVertex dest);

/**
 * \brief Copies all in-edges from source to target.
 *
 * Edge properties (aside from index) are preserved.
 */
void clone_in_edges(NGHolder &g, NFAVertex source, NFAVertex dest);

/** \brief True if the graph contains an edge from one of {start, startDs} to
 * one of {accept, acceptEod}. */
bool isVacuous(const NGHolder &h);

/** \brief True if the graph contains no floating vertices (startDs has no
 * proper successors). */
bool isAnchored(const NGHolder &h);

/** \brief True if the graph contains no anchored vertices (start has no
 * successors aside from startDs or vertices connected to startDs). */
bool isFloating(const NGHolder &h);

/** True if the graph contains no back-edges at all, other than the
 * startDs self-loop. */
bool isAcyclic(const NGHolder &g);

/** True if the graph has a cycle reachable from the given source vertex. */
bool hasReachableCycle(const NGHolder &g, NFAVertex src);

/** True if g has any cycles which are not self-loops. */
bool hasBigCycles(const NGHolder &g);

/** Returns the set of all vertices that appear in any of the graph's cycles. */
std::set<NFAVertex> findVerticesInCycles(const NGHolder &g);

bool can_never_match(const NGHolder &g);

/* \brief Does the graph have any edges leading into acceptEod (aside from
 * accept) or will it have after resolving asserts? */
bool can_match_at_eod(const NGHolder &h);

bool can_only_match_at_eod(const NGHolder &g);

/** \brief Does this graph become a "firehose", matching between every
 * byte? */
bool matches_everywhere(const NGHolder &h);


struct mbsb_cache {
    explicit mbsb_cache(const NGHolder &gg) : g(gg) {}
    std::map<std::pair<u32, u32>, bool> cache;
    const NGHolder &g;
};

/* weaker than straight domination as allows jump edges */
bool mustBeSetBefore(NFAVertex u, NFAVertex v, const NGHolder &g,
                     mbsb_cache &cache);

/* adds the literal 's' to the end of the graph before h.accept */
void appendLiteral(NGHolder &h, const ue2_literal &s);

/** \brief Fill graph \a outp with a subset of the vertices in \a in (given in
 * \a in). A vertex mapping is returned in \a v_map_out. */
void fillHolder(NGHolder *outp, const NGHolder &in,
                const std::deque<NFAVertex> &vv,
                unordered_map<NFAVertex, NFAVertex> *v_map_out);

/** \brief Clone the graph in \a in into graph \a out, returning a vertex
 * mapping in \a v_map_out. */
void cloneHolder(NGHolder &out, const NGHolder &in,
                 unordered_map<NFAVertex, NFAVertex> *v_map_out);

/** \brief Clone the graph in \a in into graph \a out. */
void cloneHolder(NGHolder &out, const NGHolder &in);

/** \brief Build a clone of graph \a in and return a pointer to it. */
std::unique_ptr<NGHolder> cloneHolder(const NGHolder &in);

/** \brief Clear all reports on vertices that do not have an edge to accept or
 * acceptEod. */
void clearReports(NGHolder &g);

/** \brief Add report \a r_new to every vertex that already has report \a
 * r_old. */
void duplicateReport(NGHolder &g, ReportID r_old, ReportID r_new);

/** Construct a reversed copy of an arbitrary NGHolder, mapping starts to
 * accepts. */
void reverseHolder(const NGHolder &g, NGHolder &out);

#ifndef NDEBUG

// Assertions: only available in internal builds.

/**
 * Used in sanity-checking assertions: returns true if all vertices
 * with edges to accept or acceptEod have at least one report ID. Additionally,
 * checks that ONLY vertices with edges to accept or acceptEod has reports.
 */
bool allMatchStatesHaveReports(const NGHolder &g);

/**
 * Assertion: returns true if the graph is triggered and all edges out of start
 * have tops OR if the graph is not-triggered and all edges out of start have no
 * tops.
 */
bool isCorrectlyTopped(const NGHolder &g);
#endif // NDEBUG

} // namespace ue2

#endif
