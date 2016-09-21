#pragma once

#include <fluid/of13msg.hh>

/**
 * This header defines some helper functions to
 * build the VLAN tags used by the Hypervisor.
 * First is an id and id_mask built which is then
 * spread out over the VLAN VID and VLAN PCP bits.
 */

/// Create a mask consisting of a variable amount of bits
constexpr uint64_t make_mask(int mask_bits) {
	return (mask_bits==1) ?
		1 :
		(make_mask(mask_bits-1)<<1 | 1 );
}

template<typename tag_type_t>
class Tag {
protected:
	/// The actual value that is set
	tag_type_t tag;
	/// The mask bits
	tag_type_t mask;

	/// Initialize an empty tag
	Tag();
	/// Initialize a tag from a known tag
	Tag(tag_type_t tag);

	/// Helper function to set a value in the tag
	template<int bits, int offset>
	void set_value(unsigned int val);
	/// Helper function to get a value in the tag
	template<int bits, int offset>
	unsigned int get_value() const;
};

/// A base VLANTag that shouldn't be
class VLANTag : public Tag<uint16_t> {
protected:

	/// Create a vlan tag without a tag or mask set
	VLANTag();
	/// Initialize a vlan tag from raw bytes
	VLANTag(uint16_t raw);

	/// The amount of bits per field
	static constexpr int num_switch_bits = 14;
	static constexpr int num_slice_bits  = 7;
	static constexpr int num_port_bits   = 7;

	/// The possible types of VLAN Tag
	enum type_t {
		switch_tag = 0,
		port_tag   = 1
	};

	/// Set the type for this VLAN Tag
	void set_type(type_t type);
	/// Get the type for this VLAN Tag
	type_t get_type() const;

	/// Create a tag from raw bytes going over the wire
	static uint16_t parse_from_raw(uint16_t raw);
public:
	static constexpr uint16_t max_slice_id  = make_mask(num_slice_bits);
	static constexpr uint16_t max_switch_id = make_mask(num_switch_bits);
	static constexpr uint16_t max_port_id   = make_mask(num_port_bits);

	/// Make the raw bytes as they go over the wire
	uint16_t make_raw() const;

	/// Add the data set in this object to the match field
	void add_to_match(fluid_msg::of13::FlowMod& flowmod) const;

	/// Add the data contained in this VLANTag to an action set
	/**
	 * This function works both for WriteActions and ApplyActions.
	 */
	template<class ActionSet>
	void add_to_actions(ActionSet& action_set) const;
};

/// A switch VLAN Tag
class SwitchVLANTag : public VLANTag {
public:
	/// Create an empty switch VLAN Tag
	SwitchVLANTag();
	/// Create a switch VLAN tag from raw bytes
	SwitchVLANTag(uint16_t raw);

	/// Set the switch value
	void set_switch(unsigned int switch_id);
	/// Get the switch value
	unsigned int get_switch() const;
};

/// A port VLAN Tag
class PortVLANTag : public VLANTag {
public:
	/// Create an empty port VLAN Tag
	PortVLANTag();
	/// Create a port VLAN Tag from raw bytes
	PortVLANTag(uint16_t raw);

	/// Set the slice value
	void set_slice(unsigned int slice_id);
	/// Get the slice value
	unsigned int get_slice() const;
	/// Set the port value
	void set_port(unsigned int port_id);
	/// Get the port value
	unsigned int get_port() const;
};

class MetadataTag : public Tag<uint64_t> {
	/// The amount of bits used to describe
	static constexpr int num_virtual_switch_bits = 13;
public:
	static constexpr uint16_t max_virtual_switch_id = make_mask(num_virtual_switch_bits);

	MetadataTag();

	/// Set the group in this tag
	void set_group(bool group);
	/// Set the virtual switch id in this tag
	void set_virtual_switch(int switch_id);

	/// Get the group bit in this tag
	bool get_group() const;
	/// Get the virtual switch id from this tag
	int get_virtual_switch() const;

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
	bool add_to_instructions(fluid_msg::of13::FlowMod& flowmod) const;
};
