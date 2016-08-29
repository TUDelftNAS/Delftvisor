#pragma once

#include <fluid/of13msg.hh>

/**
 * This header defines some helper functions to
 * build the VLAN tags used by the Hypervisor.
 * First is an id and id_mask built which is then
 * spread out over the VLAN VID and VLAN PCP bits.
 */

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

class VLANTag {
	/// The actual bits
	uint32_t tag;
	/// The mask
	uint32_t mask;

	VLANTag(uint32_t tag) :
		tag(tag),
		mask(make_mask(
			num_is_port_tag_bits +
			num_slice_bits +
			num_switch_bits)) {
	}
public:
	static constexpr uint32_t max_slice_id  = make_mask(num_slice_bits);
	static constexpr uint32_t max_switch_id = make_mask(num_switch_bits);
	static constexpr uint32_t max_port_id   = make_mask(num_port_bits);

	VLANTag() :
		tag(0),
		mask(0) {
	}

	/// Create an VLAN id from the raw 2 byte
	static VLANTag create_from_raw(uint16_t raw) {
		// Remove the 12th bit, this is the CFI bit and is always 1
		raw = (raw&make_mask(12)) |
			((raw>>1)&(make_mask(3)<<12));

		return VLANTag(raw);
	}

	/// Make the raw bytes as they go over the wire
	uint16_t make_raw() const {
		return (tag&make_mask(12)) |
			(1<<12) |
			((tag&(make_mask(3)<<12))<<1);
	}

	void set_is_port_tag(int is_port_tag_bit) {
		tag  |= (is_port_tag_bit&1)             << is_port_tag_offset;
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
		uint32_t pcp_tag  = (tag>>12)  & make_mask( 3);
		uint32_t pcp_mask = (mask>>12) & make_mask( 3);

		// Always add the vid tag so OFPVID_PRESENT bit is
		// always set which is mandatory for PCP tag matching
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

	/// Add the data contained in this VLANTag to an action set
	/**
	 * This function works both for WriteActions and ApplyActions.
	 */
	template<class ActionSet>
	void add_to_actions(ActionSet& action_set) const {
		uint32_t vid_tag = tag       & make_mask(12);
		uint32_t pcp_tag = (tag>>12) & make_mask( 3);

		action_set.add_action(
			new fluid_msg::of13::SetFieldAction(
				new fluid_msg::of13::VLANVid(
					vid_tag | fluid_msg::of13::OFPVID_PRESENT)));
		action_set.add_action(
			new fluid_msg::of13::SetFieldAction(
				new fluid_msg::of13::VLANPcp(pcp_tag)));
	}
};

class MetadataTag {
	uint64_t tag, mask;
public:
	MetadataTag() :
		tag(0),
		mask(0) {
	}

	/// Set the group in this tag
	void set_group(int group_id) {
		tag  |= (group_id&1);
		mask |= 1;
	}
	/// Set the slice data in this tag
	void set_slice(int slice_id) {
		tag  |= (slice_id&make_mask(num_slice_bits)) << 1;
		mask |= make_mask(num_slice_bits)            << 1;
	}

	/// Get the group bit in this tag
	bool get_group() const {
		return tag&1;
	}
	/// Get the slice in this tag
	int get_slice() const {
		return (tag>>1) & make_mask(num_slice_bits);
	}

	/// Add a match to this metadata to a flowmod message
	/**
	 * This function also checks if a match on metadata already
	 * exists, if it exists it shifts the existing match to the
	 * left to make room and adds the new metadata. If the
	 * existing mask would be shifted out of the mask field
	 * don't do anything and return that the operation has
	 * failed.
	 * \return If adding the match was successful
	 */
	bool add_to_match(fluid_msg::of13::FlowMod& flowmod) const {
		fluid_msg::of13::OXMTLV* oxm = flowmod.get_oxm_field(
				fluid_msg::of13::OFPXMT_OFB_METADATA);
		// If there is an existing match on metadata
		if( oxm != nullptr ) {
			fluid_msg::of13::Metadata* existing_metadata =
				(fluid_msg::of13::Metadata*) oxm;

			// If the existing value is not masked can new
			// data never be added
			if( !existing_metadata->has_mask() ) {
				return false;
			}

			// TODO Continue
		}
		return true;
	}

	/// Add a metadata tag instruction to this flowmod
	/**
	 * This function adds the instruction to the flowmod,
	 * regardless if a write-metadata instruction already
	 * exists.
	 */
	bool add_to_instruction(fluid_msg::of13::FlowMod& flowmod) const {
		flowmod.add_instruction(
			new fluid_msg::of13::WriteMetadata(
				tag,
				mask));
		return true;
	}
};
