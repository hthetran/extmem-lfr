//
// Created by hungt on 15.12.16.
//

#include <iostream>
#include <chrono>

#include <algorithm>
#include <locale>

#include <stxxl/cmdline>

#include <stack>
#include <stxxl/vector>
#include <EdgeStream.h>

#include <Utils/MonotonicPowerlawRandomStream.h>
#include <HavelHakimi/HavelHakimiIMGenerator.h>
#include <Utils/StreamPusher.h>


#include <DegreeDistributionCheck.h>
#include "SwapGenerator.h"

#include <EdgeSwaps/EdgeSwapParallelTFP.h>
#include <EdgeSwaps/ModifiedEdgeSwapTFP.h>

#include <ConfigurationModel.h>
#include <SwapStream.h>
#include <Utils/export_metis.h>

struct RunConfig {
    stxxl::uint64 numNodes;
    stxxl::uint64 minDeg;
    stxxl::uint64 maxDeg;
    double gamma;

    stxxl::uint64 index;

    unsigned int randomSeed;
    RunConfig()
            : numNodes(10 * IntScale::K)
            , minDeg(20)
            , maxDeg(1 * IntScale::K)
            , gamma(-2)
            , index(0)

    {
        using myclock = std::chrono::high_resolution_clock;
        myclock::duration d = myclock::now() - myclock::time_point::min();
        randomSeed = d.count();
    }

#if STXXL_VERSION_INTEGER > 10401
#define CMDLINE_COMP(chr, str, dest, args...) \
        chr, str, dest, args
#else
    #define CMDLINE_COMP(chr, str, dest, args...) \
        chr, str, args, dest
#endif

    bool parse_cmdline(int argc, char* argv[]) {
        stxxl::cmdline_parser cp;

        // setup and gather parameters
        {
            cp.add_bytes (CMDLINE_COMP('n', "num-nodes", numNodes,     "Generate # nodes, Default: 10 Mi"));
            cp.add_bytes (CMDLINE_COMP('a', "min-deg",   minDeg,       "Min. Deg of Powerlaw Deg. Distr."));
            cp.add_bytes (CMDLINE_COMP('b', "max-deg",   maxDeg,       "Max. Deg of Powerlaw Deg. Distr."));
            cp.add_double(CMDLINE_COMP('g', "gamma",     gamma,        "Gamma of Powerlaw Deg. Distr."));
            cp.add_uint  (CMDLINE_COMP('s', "seed",      randomSeed,   "Initial seed for PRNG"));
            cp.add_bytes (CMDLINE_COMP('j', "index",     index,        "Index"));

            if (!cp.process(argc, (const char *const *) argv)) {
                cp.print_usage();
                return false;
            }
        }

        cp.print_result();
        return true;
    }
};





void benchmark(RunConfig & config) {
    stxxl::stats *stats = stxxl::stats::get_instance();
    stxxl::stats_data stats_begin(*stats);

    // Build edge list
    EdgeStream edge_stream;
    SwapStream swap_stream;

    // prepare generator
    HavelHakimiIMGenerator hh_gen(HavelHakimiIMGenerator::PushDirection::DecreasingDegree);
    MonotonicPowerlawRandomStream<false> degreeSequence(config.minDeg, config.maxDeg, config.gamma, config.numNodes);
    StreamPusher<decltype(degreeSequence), decltype(hh_gen)>(degreeSequence, hh_gen);
    hh_gen.generate();

    std::cout << (stxxl::stats_data(*stats) - stats_begin);
    export_as_metis(hh_gen, "graph.metis");
}

int main(int argc, char* argv[]) {
#ifndef NDEBUG
    std::cout << "[build with assertions]" << std::endl;
#endif
    std::cout << "STXXL VERSION: " << STXXL_VERSION_INTEGER << std::endl;

    // nice to have in logs to restart it easier
    for(int i=0; i < argc; ++i)
        std::cout << argv[i] << " ";
    std::cout << std::endl;

    // infos regarding data types
    std::cout << "int_t:         " << sizeof(int_t) << "b\n"
            "uint_t:        " << sizeof(uint_t) << "b\n"
                      "node_t:        " << sizeof(node_t) << "b\n"
                      "degree_t:      " << sizeof(degree_t) << "b\n"
                      "community_t:   " << sizeof(community_t) << "b\n"
                      "edge_t:        " << sizeof(edge_t) << "b\n"
                      "swapid_t:      " << sizeof(swapid_t) << "b\n"
                      "SwapDescriptor:" << sizeof(SwapDescriptor) << "b\n"
              << std::endl;

    RunConfig config;
    if (!config.parse_cmdline(argc, argv))
        return -1;

    stxxl::srandom_number32(config.randomSeed);
    stxxl::set_seed(config.randomSeed);

    benchmark(config);
    std::cout << "Maximum EM allocation: " <<  stxxl::block_manager::get_instance()->get_maximum_allocation() << std::endl;


    return 0;
}
