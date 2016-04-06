#include <iostream>
#include <chrono>

#include <string>
#include <algorithm>
#include <locale>

#include <stxxl/cmdline>

#include <stack>
#include <stxxl/vector>

#include <Utils/IOStatistics.h>

#include <Utils/MonotonicPowerlawRandomStream.h>
#include <HavelHakimi/HavelHakimiIMGenerator.h>
#include <Utils/StreamPusher.h>


#include <DegreeDistributionCheck.h>
#include "SwapGenerator.h"

#include <EdgeSwaps/EdgeSwapParallelTFP.h>
#include <EdgeSwaps/EdgeSwapInternalSwaps.h>
#include <EdgeSwaps/EdgeSwapTFP.h>
#include <EdgeSwaps/IMEdgeSwap.h>

enum EdgeSwapAlgo {
    IM,
    SEMI, // InternalSwaps
    TFP,
    PTFP
};

struct RunConfig {
    stxxl::uint64 numNodes;
    stxxl::uint64 minDeg;
    stxxl::uint64 maxDeg;
    double gamma;

    stxxl::uint64 numSwaps;
    stxxl::uint64 runSize;
    stxxl::uint64 batchSize;

    unsigned int randomSeed;

    EdgeSwapAlgo edgeSwapAlgo;

    RunConfig() 
        : numNodes(10 * IntScale::Mi)
        , minDeg(2)
        , maxDeg(100000)
        , gamma(-2.0)

        , numSwaps(numNodes)
        , runSize(numNodes/10)
        , batchSize(IntScale::Mi)


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
        std::string swap_algo_name;

        // setup and gather parameters
        {
            cp.add_bytes (CMDLINE_COMP('n', "num-nodes", numNodes, "Generate # nodes, Default: 10 Mi"));
            cp.add_bytes (CMDLINE_COMP('a', "min-deg",   minDeg,   "Min. Deg of Powerlaw Deg. Distr."));
            cp.add_bytes (CMDLINE_COMP('b', "max-deg",   maxDeg,   "Max. Deg of Powerlaw Deg. Distr."));
            cp.add_double(CMDLINE_COMP('g', "gamma",     gamma,    "Gamma of Powerlaw Deg. Distr."));
            cp.add_uint  (CMDLINE_COMP('s', "seed",      randomSeed,   "Initial seed for PRNG"));

            cp.add_bytes  (CMDLINE_COMP('m', "num-swaps", numSwaps,   "Number of swaps to perform"));
            cp.add_bytes  (CMDLINE_COMP('r', "run-size", runSize, "Number of swaps per graph scan"));
            cp.add_bytes  (CMDLINE_COMP('k', "batch-size", batchSize, "Batch size of PTFP"));

            cp.add_string(CMDLINE_COMP('e', "swap-algo", swap_algo_name, "SwapAlgo to use: IM, SEMI, TFP, PTFP (default)"));

            if (!cp.process(argc, argv)) {
                cp.print_usage();
                return false;
            }
        }

        // select edge swap algo
        {
            std::transform(swap_algo_name.begin(), swap_algo_name.end(), swap_algo_name.begin(), ::toupper);

            if      (swap_algo_name.empty() ||
                     0 == swap_algo_name.compare("PTFP")) { edgeSwapAlgo = PTFP; }
            else if (0 == swap_algo_name.compare("TFP"))  { edgeSwapAlgo = TFP; }
            else if (0 == swap_algo_name.compare("SEMI")) { edgeSwapAlgo = SEMI; }
            else if (0 == swap_algo_name.compare("IM"))   { edgeSwapAlgo = IM; }
            else {
                std::cerr << "Invalid edge swap algorithm specified: " << swap_algo_name << std::endl;
                cp.print_usage();
                return false;
            }
            std::cout << "Using edge swap algo: " << swap_algo_name << std::endl;
        }


        cp.print_result();
        return true;
    }
};





void benchmark(RunConfig & config) {
    stxxl::stats *stats = stxxl::stats::get_instance();
    stxxl::stats_data stats_begin(*stats);

    // Build edge list
    using edge_vector_t = stxxl::VECTOR_GENERATOR<edge_t>::result;
    edge_vector_t edges;
    {
        IOStatistics hh_report("HHEdges");

        // prepare generator
        HavelHakimiIMGenerator hh_gen(HavelHakimiIMGenerator::PushDirection::DecreasingDegree);
        MonotonicPowerlawRandomStream<false> degreeSequence(config.minDeg, config.maxDeg, config.gamma, config.numNodes);
        StreamPusher<decltype(degreeSequence), decltype(hh_gen)>(degreeSequence, hh_gen);
        hh_gen.generate();

        // materialize stream
        edges.resize(hh_gen.maxEdges());
        auto endIt = stxxl::stream::materialize(hh_gen, edges.begin());
        edges.resize(endIt - edges.begin());

        std::cout << "Generated " << edges.size() << " edges\n";
    }

    // Build swaps
    SwapGenerator swap_gen(config.numSwaps, edges.size());
    using swap_vector_t = stxxl::VECTOR_GENERATOR<SwapDescriptor>::result;
    swap_vector_t swaps;
    if (config.edgeSwapAlgo == TFP) {
        IOStatistics swap_report("SwapGenerator");
        swaps.resize(config.numSwaps);

        auto endIt = stxxl::stream::materialize(swap_gen, swaps.begin());
        assert(static_cast<size_t>(endIt - swaps.begin()) == config.numSwaps);

    } else {
        std::cout << "Swap Algo accepts swaps as stream\n";
    }

    // Perform edge swaps
    {

        IOStatistics swap_report("SwapStats");
        switch (config.edgeSwapAlgo) {
            case IM: {
                IMEdgeSwap swap_algo(edges);
                StreamPusher<decltype(swap_gen), decltype(swap_algo)>(swap_gen, swap_algo);
                swap_algo.run();
                break;
            }

            case SEMI: {
                EdgeSwapInternalSwaps swap_algo(edges, config.runSize);
                StreamPusher<decltype(swap_gen), decltype(swap_algo)>(swap_gen, swap_algo);
                swap_algo.run();
                break;
            }

            case TFP: {
                EdgeSwapTFP::EdgeSwapTFP TFPSwaps(edges, swaps);
                TFPSwaps.run(config.runSize);
                break;
            }


            case PTFP: {
                EdgeSwapParallelTFP::EdgeSwapParallelTFP swap_algo(edges, config.runSize);
                StreamPusher<decltype(swap_gen), decltype(swap_algo)>(swap_gen, swap_algo);
                swap_algo.run();
                break;
            }

        }
    }
}




int main(int argc, char* argv[]) {
#ifndef NDEBUG
    std::cout << "[build with assertions]" << std::endl;
#endif
    std::cout << "STXXL VERSION: " << STXXL_VERSION_INTEGER << std::endl;

    RunConfig config;
    if (!config.parse_cmdline(argc, argv))
        return -1;

    stxxl::srandom_number32(config.randomSeed);
    stxxl::set_seed(config.randomSeed);

    benchmark(config);

    return 0;
}
