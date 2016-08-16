#pragma once

namespace rewrite {
	constexpr int num_switch_bits = 5;
	constexpr int num_slice_bits  = 5;
	constexpr int num_port_bits   = 5;

	/// Create a mask consisting of a variable amount of bits
	constexpr uint32_t make_mask(int mask_bits) {
		return (mask_bits==1) ?
			1 :
			(make_mask(mask_bits-1)<<1 | 1 );
	}

	constexpr int switch_offset = 0;
	constexpr int slice_offset  = num_switch_bits;
	constexpr int port_offset   = num_switch_bits + num_slice_bits;

	constexpr uint32_t switch_mask = make_mask(num_switch_bits) << switch_offset;
	constexpr uint32_t slice_mask  = make_mask(num_slice_bits)  << slice_offset;
	constexpr uint32_t port_mask   = make_mask(num_port_bits)   << port_offset;

	constexpr uint32_t max_switch_id = make_mask(num_switch_bits);
	constexpr uint32_t max_slice_id  = make_mask(num_slice_bits);
	constexpr uint32_t max_port_id   = make_mask(num_port_bits);

	constexpr uint32_t switch_bits(int switch_id) {
		return (switch_id<<switch_offset) & switch_mask;
	}
	constexpr uint32_t slice_bits(int slice_id) {
		return (slice_id<<slice_offset) & slice_mask;
	}
	constexpr uint32_t port_bits(int port_id) {
		return (port_id<<port_offset) & port_mask;
	}

	constexpr int extract_switch_id( uint32_t id ) {
		return (id&switch_mask) >> switch_offset;
	}
	constexpr int extract_slice_id( uint32_t id ) {
		return (id&slice_mask) >> slice_offset;
	}
	constexpr int extract_port_id( uint32_t id ) {
		return (id&port_mask) >> port_offset;
	}
}
