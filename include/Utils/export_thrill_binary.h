#pragma once

#include <EdgeStream.h>
#include <fstream>
#include <stxxl/sorter>
#include <defs.h>
#include <GenericComparator.h>
#include <stack>

/*!
 * CRTP class to enhance item/memory writer classes with Varint encoding and
 * String encoding.
 */
    //! Append a varint to the writer.
template <typename stream>
size_t PutVarint(stream& s, uint64_t v) {
	if (v < 128) {
		s << uint8_t(v);
		return 1;
	}
	if (v < 128 * 128) {
		s << uint8_t(((v >> 00) & 0x7F) | 0x80);
		s << uint8_t((v >> 07) & 0x7F);
		return 2;
	}
	if (v < 128 * 128 * 128) {
		s << uint8_t(((v >> 00) & 0x7F) | 0x80);
		s << uint8_t(((v >> 07) & 0x7F) | 0x80);
		s << uint8_t((v >> 14) & 0x7F);
		return 3;
	}
	if (v < 128 * 128 * 128 * 128) {
		s << uint8_t(((v >> 00) & 0x7F) | 0x80);
		s << uint8_t(((v >> 07) & 0x7F) | 0x80);
		s << uint8_t(((v >> 14) & 0x7F) | 0x80);
		s << uint8_t((v >> 21) & 0x7F);
		return 4;
	}
	if (v < 128llu * 128 * 128 * 128 * 128) {
		s << uint8_t(((v >> 00) & 0x7F) | 0x80);
		s << uint8_t(((v >> 07) & 0x7F) | 0x80);
		s << uint8_t(((v >> 14) & 0x7F) | 0x80);
		s << uint8_t(((v >> 21) & 0x7F) | 0x80);
		s << uint8_t((v >> 28) & 0x7F);
		return 5;
	}
	if (v < 128llu * 128 * 128 * 128 * 128 * 128) {
		s << uint8_t(((v >> 00) & 0x7F) | 0x80);
		s << uint8_t(((v >> 07) & 0x7F) | 0x80);
		s << uint8_t(((v >> 14) & 0x7F) | 0x80);
		s << uint8_t(((v >> 21) & 0x7F) | 0x80);
		s << uint8_t(((v >> 28) & 0x7F) | 0x80);
		s << uint8_t((v >> 35) & 0x7F);
		return 6;
	}
	if (v < 128llu * 128 * 128 * 128 * 128 * 128 * 128) {
		s << uint8_t(((v >> 00) & 0x7F) | 0x80);
		s << uint8_t(((v >> 07) & 0x7F) | 0x80);
		s << uint8_t(((v >> 14) & 0x7F) | 0x80);
		s << uint8_t(((v >> 21) & 0x7F) | 0x80);
		s << uint8_t(((v >> 28) & 0x7F) | 0x80);
		s << uint8_t(((v >> 35) & 0x7F) | 0x80);
		s << uint8_t((v >> 42) & 0x7F);
		return 7;
	}
	if (v < 128llu * 128 * 128 * 128 * 128 * 128 * 128 * 128) {
		s << uint8_t(((v >> 00) & 0x7F) | 0x80);
		s << uint8_t(((v >> 07) & 0x7F) | 0x80);
		s << uint8_t(((v >> 14) & 0x7F) | 0x80);
		s << uint8_t(((v >> 21) & 0x7F) | 0x80);
		s << uint8_t(((v >> 28) & 0x7F) | 0x80);
		s << uint8_t(((v >> 35) & 0x7F) | 0x80);
		s << uint8_t(((v >> 42) & 0x7F) | 0x80);
		s << uint8_t((v >> 49) & 0x7F);
		return 8;
	}
	if (v < 128llu * 128 * 128 * 128 * 128 * 128 * 128 * 128 * 128) {
		s << uint8_t(((v >> 00) & 0x7F) | 0x80);
		s << uint8_t(((v >> 07) & 0x7F) | 0x80);
		s << uint8_t(((v >> 14) & 0x7F) | 0x80);
		s << uint8_t(((v >> 21) & 0x7F) | 0x80);
		s << uint8_t(((v >> 28) & 0x7F) | 0x80);
		s << uint8_t(((v >> 35) & 0x7F) | 0x80);
		s << uint8_t(((v >> 42) & 0x7F) | 0x80);
		s << uint8_t(((v >> 49) & 0x7F) | 0x80);
		s << uint8_t((v >> 56) & 0x7F);
		return 9;
	}

	s << uint8_t(((v >> 00) & 0x7F) | 0x80);
	s << uint8_t(((v >> 07) & 0x7F) | 0x80);
	s << uint8_t(((v >> 14) & 0x7F) | 0x80);
	s << uint8_t(((v >> 21) & 0x7F) | 0x80);
	s << uint8_t(((v >> 28) & 0x7F) | 0x80);
	s << uint8_t(((v >> 35) & 0x7F) | 0x80);
	s << uint8_t(((v >> 42) & 0x7F) | 0x80);
	s << uint8_t(((v >> 49) & 0x7F) | 0x80);
	s << uint8_t(((v >> 56) & 0x7F) | 0x80);
	s << uint8_t((v >> 63) & 0x7F);
	return 10;
};

void export_as_thrill_binary(EdgeStream &edges, node_t num_nodes, const std::string& filename, stxxl::external_size_type max_bytes = (1ul<<30)) {
	edges.rewind();

	size_t file_number = 0;

	auto next_filename = [&]() {
	    std::stringstream ss;
	    ss << filename << ".part-" << std::setw(5) << std::setfill('0') << file_number;
	    ++file_number;
	    return ss.str();
	};

	std::ofstream out_stream(next_filename(), std::ios::trunc | std::ios::binary);
	//out_stream << num_nodes << " " << num_edges << " " << 0 << std::endl;
	
	std::vector<node_t> neighbors;
	stxxl::external_size_type bytes_written = 0;
	for (node_t u = 0; u < num_nodes; ++u) {
		neighbors.clear();
		for (; !edges.empty() && edges->first == u; ++edges) {
			neighbors.push_back(edges->second);
		}

		// assume the size of the neighbors needs 4 bytes
		if (bytes_written > 0 && bytes_written + neighbors.size() * 4 + 4 > max_bytes) {
		    out_stream.close();
		    // This does not compile with GCC < 5 because of a missing move assignment operator!
		    // see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54316 for a related issue
		    out_stream = std::ofstream(next_filename(), std::ios::trunc | std::ios::binary);
		    bytes_written = 0;
		}

		bytes_written += 4llu * neighbors.size();
		bytes_written += PutVarint(out_stream, neighbors.size());

		for (node_t v : neighbors) {
			static_assert(std::is_same<int32_t, node_t>::value, "Node type is not int32 anymore, adjust code!");
			out_stream.write(reinterpret_cast<const char*>(&v), 4);
		}
	}

	out_stream.close();
};
