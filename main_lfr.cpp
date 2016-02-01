#include <iostream>
#include <chrono>

#include <stxxl/cmdline>

#include <defs.h>
#include <PowerlawDegreeSequence.h>
#include <LFR/LFR.h>

class RunConfig {
    void _update_structs() {
        node_distribution_param.exponent = node_gamma;
        node_distribution_param.minDegree = node_min_degree;
        node_distribution_param.maxDegree = node_max_degree;
        node_distribution_param.numberOfNodes = number_of_nodes;

        community_distribution_param.exponent = community_gamma;
        community_distribution_param.minDegree = community_min_members;
        community_distribution_param.maxDegree = community_max_members;
        community_distribution_param.numberOfNodes = number_of_communities;
    }

public:
    stxxl::uint64 number_of_nodes;
    stxxl::uint64 number_of_communities;

    stxxl::uint64 node_min_degree;
    stxxl::uint64 node_max_degree;
    stxxl::uint64 max_degree_within_community;
    double node_gamma;

    stxxl::uint64 community_min_members;
    stxxl::uint64 community_max_members;
    double community_gamma;

    double mixing;
    unsigned int randomSeed;

    PowerlawDegreeSequence::Parameters node_distribution_param;
    PowerlawDegreeSequence::Parameters community_distribution_param;

    RunConfig() :
        number_of_nodes      (1000000),
        number_of_communities(  10000),
        node_min_degree(10),
        node_max_degree(number_of_nodes/10),
        max_degree_within_community(node_max_degree),
        node_gamma(-2.0),
        community_min_members(    100),
        community_max_members(1000000),
        community_gamma(-2.0),
        mixing(0.5)
    {
        using myclock = std::chrono::high_resolution_clock;
        myclock::duration d = myclock::now() - myclock::time_point::min();
        randomSeed = d.count();
        _update_structs();
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

        cp.add_bytes (CMDLINE_COMP('n', "num-nodes", number_of_nodes, "Number of nodes"));
        cp.add_bytes (CMDLINE_COMP('c', "num-communities", number_of_communities, "Number of communities"));

        cp.add_bytes (CMDLINE_COMP('h', "node-min-degree",   node_min_degree,   "Minumum node degree"));
        cp.add_bytes (CMDLINE_COMP('i', "node-max-degree",   node_max_degree,   "Maximum node degree"));
        cp.add_double(CMDLINE_COMP('j', "node-gamma",        node_gamma,        "Exponent of node degree distribution"));
        cp.add_bytes (CMDLINE_COMP('k', "max_degree_within_community",   max_degree_within_community,   "Maximum intra-community degree"));

        cp.add_bytes (CMDLINE_COMP('x', "community-min-members",   community_min_members,   "Minumum community size"));
        cp.add_bytes (CMDLINE_COMP('y', "community-max-members",   community_max_members,   "Maximum community size"));
        cp.add_double(CMDLINE_COMP('z', "community-gamma",         community_gamma,         "Exponent of community size distribution"));

        cp.add_uint  (CMDLINE_COMP('s', "seed",      randomSeed,   "Initial seed for PRNG"));

        cp.add_double(CMDLINE_COMP('m', "mixing",        mixing,         "Fraction node edge being inter-community"));

        assert(number_of_communities < std::numeric_limits<community_t>::max());

        if (!cp.process(argc, argv)) {
            cp.print_usage();
            return false;
        }

        cp.print_result();

        _update_structs();

        return true;
    }
};

int main(int argc, char* argv[]) {
#ifndef NDEBUG
    std::cout << "[build with assertions]" << std::endl;
#endif

    RunConfig config;
    if (!config.parse_cmdline(argc, argv))
        return -1;

    stxxl::srandom_number32(config.randomSeed);
    stxxl::set_seed(config.randomSeed);

    LFR::LFR lfr(config.node_distribution_param,
                 config.community_distribution_param,
                 config.mixing);
    lfr.setMaxDegreeWithinCommunity(config.max_degree_within_community);
    lfr.run();

    return 0;
}