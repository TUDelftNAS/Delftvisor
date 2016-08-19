#pragma once

#include <fluid/of13msg.hh>

/**
 * This header defines some helper functions to
 * build the VLAN tags used by the Hypervisor.
 * First is an id and id_mask built which is then
 * spread out over the VLAN VID and VLAN PCP bits.
 */

namespace vlan_tag {
	constexpr int num_is_port_tag_bits = 1;
	constexpr int num_slice_bits       = 7;
	constexpr int num_switch_bits      = 7;
	constexpr int num_port_bits        = num_switch_bits;

	constexpr int is_port_tag_offset = 0;
	constexpr int slice_offset       = 1;
	constexpr int switch_offset      = 1 + num_slice_bits;
	constexpr int port_offset        = switch_offset;

	/// Create a mask consisting of a variable amount of bits
	constexpr uint32_t make_mask(int mask_bits) {
		return (mask_bits==1) ?
			1 :
			(make_mask(mask_bits-1)<<1 | 1 );
	}

	constexpr uint32_t max_slice_id  = make_mask(num_slice_bits);
	constexpr uint32_t max_switch_id = make_mask(num_switch_bits);
	constexpr uint32_t max_port_id   = make_mask(num_port_bits);

	/// An VLAN tag id
	/**
	 * This builds the match fields for
	 * VLAN tags.
	 */
	class Id {
		uint32_t tag, mask;

		Id(uint32_t tag) :
			tag(tag),
			mask(make_mask(
				num_is_port_tag_bits +
				num_slice_bits +
				num_switch_bits)) {
		}
	public:
		Id() :
			tag(0),
			mask(0) {
		}

		/// Create an VLAN id from the raw 2 byte
		static Id create_from_raw(uint16_t raw) {
			// Remove the 12th bit, this is the CFI bit and is always 1
			raw = (raw&make_mask(12)) |
				((raw>>1)&(make_mask(3)<<12));

			return Id(raw);
		}

		/// Make the raw bytes as they go over the wire
		uint16_t make_raw() const {
			return (tag&make_mask(12)) |
				(1<<12) |
				((tag&(make_mask(3)<<12))<<1);
		}

		void set_is_port_tag(int is_port_tag_bit) {
			tag  |= (is_port_tag_bit&1)         << is_port_tag_offset;
			mask |= make_mask(num_is_port_tag_bits) << is_port_tag_offset;
		}
		void set_slice(int slice_id) {
			tag  |= (slice_id&make_mask(num_slice_bits)) << slice_offset;
			mask |= make_mask(num_slice_bits)            << slice_offset;
		}
		void set_switch(int switch_id) {
			tag  |= (switch_id&make_mask(num_switch_bits)) << switch_offset;
			mask |= make_mask(num_switch_bits)             << switch_offset;
		}

		int get_is_port_tag() const {
			return (tag>>is_port_tag_offset)&1;
		}
		int get_slice() const {
			return (tag>>slice_offset)&make_mask(num_slice_bits);
		}
		int get_switch() const {
			return (tag>>switch_offset)&make_mask(num_switch_bits);
		}

		/// Add the data set in this object to the match field
		void add_to_match(fluid_msg::of13::FlowMod& flowmod) const {
			uint32_t vid_tag  = tag        & make_mask(12);
			uint32_t vid_mask = mask       & make_mask(12);
			uint32_t pcp_tag  = (tag>>12)  & make_mask(3);
			uint32_t pcp_mask = (mask>>12) & make_mask(3);

			flowmod.add_oxm_field(
				new fluid_msg::of13::VLANVid(
					vid_tag  | fluid_msg::of13::OFPVID_PRESENT,
					vid_mask | fluid_msg::of13::OFPVID_PRESENT));

			if( pcp_mask != 0 ) {
				// TODO The PCP bits aren't maskable, throw an error
				// if the pcp_mask isn't make_mask(3)
				flowmod.add_oxm_field(
					new fluid_msg::of13::VLANPcp(
						pcp_tag));
			}
		}
	};
}
