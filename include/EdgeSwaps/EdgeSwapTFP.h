#pragma once

#include <stxxl/vector>
#include <stxxl/sorter>
#include <stxxl/bits/unused.h>

#include <thread>

#include <defs.h>
#include "Swaps.h"
#include "GenericComparator.h"
#include "TupleHelper.h"

#include "EdgeSwapBase.h"
#include "BoolStream.h"

namespace EdgeSwapTFP {
    struct DependencyChainEdgeMsg {
        swapid_t swap_id;
        edgeid_t edge_id;
        edge_t edge;

        DependencyChainEdgeMsg() { }

        DependencyChainEdgeMsg(const swapid_t &swap_id_, const edgeid_t &edge_id_, const edge_t &edge_)
              : swap_id(swap_id_), edge_id(edge_id_), edge(edge_) { }

        DECL_LEX_COMPARE_OS(DependencyChainEdgeMsg, swap_id, edge_id, edge);
    };

    struct DependencyChainSuccessorMsg {
        swapid_t swap_id;
        edgeid_t edge_id;
        swapid_t successor;

        DependencyChainSuccessorMsg() { }

        DependencyChainSuccessorMsg(const swapid_t &swap_id_, const edgeid_t &edge_id_, const swapid_t &successor_) :
              swap_id(swap_id_), edge_id(edge_id_), successor(successor_) { }

        DECL_LEX_COMPARE_OS(DependencyChainSuccessorMsg, swap_id, edge_id, successor);
    };

    struct ExistenceRequestMsg {
        edge_t edge;
        swapid_t swap_id;
        bool forward_only;

        ExistenceRequestMsg() { }

        ExistenceRequestMsg(const edge_t &edge_, const swapid_t &swap_id_, const bool &forward_only_) :
              edge(edge_), swap_id(swap_id_),  forward_only(forward_only_) { }

        bool operator< (const ExistenceRequestMsg& o) const {
            return (edge < o.edge || (edge == o.edge && (swap_id > o.swap_id || (swap_id == o.swap_id && forward_only < o.forward_only))));
        }
        DECL_TO_TUPLE(edge, swap_id, forward_only);
        DECL_TUPLE_OS(ExistenceRequestMsg);
    };

    struct ExistenceInfoMsg {
        swapid_t swap_id;
        edge_t edge;
      #ifndef NDEBUG
        bool exists;
      #endif

        ExistenceInfoMsg() { }

        ExistenceInfoMsg(const swapid_t &swap_id_, const edge_t &edge_, const bool &exists_ = true) :
            swap_id(swap_id_), edge(edge_)
            #ifndef NDEBUG
            , exists(exists_)
            #endif
        { stxxl::STXXL_UNUSED(exists_); }

        DECL_LEX_COMPARE_OS(ExistenceInfoMsg, swap_id, edge
            #ifndef NDEBUG
            , exists
            #endif
        );
    };

    struct ExistenceSuccessorMsg {
        swapid_t swap_id;
        edge_t edge;
        swapid_t successor;

        ExistenceSuccessorMsg() { }

        ExistenceSuccessorMsg(const swapid_t &swap_id_, const edge_t &edge_, const swapid_t &successor_) :
              swap_id(swap_id_), edge(edge_), successor(successor_) { }

        DECL_LEX_COMPARE_OS(ExistenceSuccessorMsg, swap_id, edge, successor);
    };

    class EdgeSwapTFP : public EdgeSwapBase {
    protected:
        constexpr static size_t _pq_mem = PQ_INT_MEM;
        constexpr static size_t _pq_pool_mem = PQ_POOL_MEM;
        constexpr static size_t _sorter_mem = SORTER_MEM;

        constexpr static bool compute_stats = false;
        constexpr static bool produce_debug_vector=true;
        constexpr static bool _async_processing = false;

        edge_vector &_edges;
        swap_vector &_swaps;

        typename swap_vector::iterator _swaps_begin;
        typename swap_vector::iterator _swaps_end;
        std::unique_ptr<std::thread> _result_thread;

// swap -> edge
        using EdgeSwapMsg = std::tuple<edgeid_t, swapid_t>;
        using EdgeSwapSorter = stxxl::sorter<EdgeSwapMsg, GenericComparatorTuple<EdgeSwapMsg>::Ascending>;
        EdgeSwapSorter _edge_swap_sorter;

// dependency chain
        // we need to use a desc-comparator since the pq puts the largest element on top
        using DependencyChainEdgeComparatorSorter = typename GenericComparatorStruct<DependencyChainEdgeMsg>::Ascending;
        using DependencyChainEdgeSorter = stxxl::sorter<DependencyChainEdgeMsg, DependencyChainEdgeComparatorSorter>;
        DependencyChainEdgeSorter _depchain_edge_sorter;

        using DependencyChainSuccessorComparator = typename GenericComparatorStruct<DependencyChainSuccessorMsg>::Ascending;
        using DependencyChainSuccessorSorter = stxxl::sorter<DependencyChainSuccessorMsg, DependencyChainSuccessorComparator>;
        DependencyChainSuccessorSorter _depchain_successor_sorter;

        std::unique_ptr<std::thread> _depchain_thread;

        using EdgeIdVector = stxxl::VECTOR_GENERATOR<edgeid_t>::result;

// existence requests
        using ExistenceRequestComparator = typename GenericComparatorStruct<ExistenceRequestMsg>::Ascending;
        using ExistenceRequestSorter = stxxl::sorter<ExistenceRequestMsg, ExistenceRequestComparator>;
        ExistenceRequestSorter _existence_request_sorter;

// existence information and dependencies
        using ExistenceInfoComparator = typename GenericComparatorStruct<ExistenceInfoMsg>::Ascending;
        using ExistenceInfoSorter = stxxl::sorter<ExistenceInfoMsg, ExistenceInfoComparator>;
        ExistenceInfoSorter _existence_info_sorter;

        using ExistenceSuccessorComparator = typename GenericComparatorStruct<ExistenceSuccessorMsg>::Ascending;
        using ExistenceSuccessorSorter = stxxl::sorter<ExistenceSuccessorMsg, ExistenceSuccessorComparator>;
        ExistenceSuccessorSorter _existence_successor_sorter;

// edge updates
        using EdgeUpdateComparator = typename GenericComparator<edge_t>::Ascending;
        using EdgeUpdateSorter = stxxl::sorter<edge_t, EdgeUpdateComparator>;
        EdgeUpdateSorter _edge_update_sorter;
        std::unique_ptr<std::thread> _edge_update_sorter_thread;

// algos
        void _gather_edges();

        template <class EdgeReader>
        void _compute_dependency_chain(EdgeReader&, BoolStream&);

        void _compute_conflicts();
        void _process_existence_requests();
        void _perform_swaps();
        void _apply_updates();

        void _reset() {
            _edge_swap_sorter.clear();
            _depchain_edge_sorter.clear();
            _depchain_successor_sorter.clear();
            _existence_request_sorter.clear();
            _existence_info_sorter.clear();
            _existence_successor_sorter.clear();
        }

    public:
        EdgeSwapTFP() = delete;
        EdgeSwapTFP(const EdgeSwapTFP &) = delete;

        //! Swaps are performed during constructor.
        //! @param edges  Edge vector changed in-place
        //! @param swaps  Read-only swap vector
        EdgeSwapTFP(edge_vector &edges, swap_vector &swaps) :
              EdgeSwapBase(),
              _edges(edges),
              _swaps(swaps),

              _edge_swap_sorter(GenericComparatorTuple<EdgeSwapMsg>::Ascending(), _sorter_mem),
              _depchain_edge_sorter(DependencyChainEdgeComparatorSorter{}, _sorter_mem),
              _depchain_successor_sorter(DependencyChainSuccessorComparator{}, _sorter_mem),
              _existence_request_sorter(ExistenceRequestComparator{}, _sorter_mem),
              _existence_info_sorter(ExistenceInfoComparator{}, _sorter_mem),
              _existence_successor_sorter(ExistenceSuccessorComparator{}, _sorter_mem),
              _edge_update_sorter(EdgeUpdateComparator{}, _sorter_mem) { }

        void run(uint64_t swaps_per_iteration = 0);
    };
}