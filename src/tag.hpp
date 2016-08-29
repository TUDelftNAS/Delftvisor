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

	VLANTag();

	/// Create an VLAN id from the raw 2 byte
	static VLANTag create_from_raw(uint16_t raw);

	/// Make the raw bytes as they go over the wire
	uint16_t make_raw() const;

	void set_is_port_tag(int is_port_tag_bit);
	void set_slice(int slice_id);
	void set_switch(int switch_id);

	int get_is_port_tag() const;
	int get_slice() const;
	int get_switch() const;

	/// Add the data set in this object to the match field
	void add_to_match(fluid_msg::of13::FlowMod& flowmod) const;

	/// Add the data contained in this VLANTag to an action set
	/**
	 * This function works both for WriteActions and ApplyActions.
	 */
	template<class ActionSet>
	void add_to_actions(ActionSet& action_set) const;
};

class MetadataTag {
	uint64_t tag, mask;
public:
	MetadataTag();

	/// Set the group in this tag
	void set_group(int group_id);
	/// Set the slice data in this tag
	void set_slice(int slice_id);

	/// Get the group bit in this tag
	bool get_group() const;
	/// Get the slice in this tag
	int get_slice() const;

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
	bool add_to_match(fluid_msg::of13::FlowMod& flowmod) const;

	/// Add a metadata tag instruction to this flowmod
	/**
	 * This function adds the instruction to the flowmod,
	 * regardless if a write-metadata instruction already
	 * exists.
	 */
	bool add_to_instruction(fluid_msg::of13::FlowMod& flowmod) const;
};
