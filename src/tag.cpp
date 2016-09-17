#include "tag.hpp"

template<typename tag_type_t>
template<int bits, int offset>
void Tag<tag_type_t>::set_value(unsigned int val) {
	// Remove the old value
	tag  &= ~(make_mask(bits) << offset);
	// Set the new value
	tag  |= (val&make_mask(bits)) << offset;
	// Update the mask
	mask |= make_mask(bits) << offset;
}

template<typename tag_type_t>
template<int bits, int offset>
unsigned int Tag<tag_type_t>::get_value() const {
	return (tag>>offset) & make_mask(bits);
}

template<typename tag_type_t>
Tag<tag_type_t>::Tag() :
	tag(0),
	mask(0) {
}

VLANTag::VLANTag() :
	Tag<uint16_t>::Tag() {
}

VLANTag::VLANTag(uint16_t raw) {
	mask = make_mask(15);
	tag  = (raw&make_mask(12)) |
		((raw>>1)&(make_mask(3)<<12));
}

void VLANTag::set_type(type_t type) {
	set_value<1,0>(type);
}

VLANTag::type_t VLANTag::get_type() const {
	return (VLANTag::type_t)get_value<1,0>();
}

uint16_t VLANTag::make_raw() const {
	// This assumes tag&mask==tag
	return (tag&make_mask(12)) |
		(1<<12) |
		((tag&(make_mask(3)<<12))<<1);
}

void VLANTag::add_to_match(fluid_msg::of13::FlowMod& flowmod) const {
	uint16_t vid_tag  = tag        & make_mask(12);
	uint16_t vid_mask = mask       & make_mask(12);
	uint16_t pcp_tag  = (tag>>12)  & make_mask( 3);
	uint16_t pcp_mask = (mask>>12) & make_mask( 3);

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

template<class ActionSet>
void VLANTag::add_to_actions(ActionSet& action_set) const {
	uint16_t vid_tag = tag       & make_mask(12);
	uint16_t pcp_tag = (tag>>12) & make_mask( 3);

	action_set.add_action(
		new fluid_msg::of13::SetFieldAction(
			new fluid_msg::of13::VLANVid(
				vid_tag | fluid_msg::of13::OFPVID_PRESENT)));
	action_set.add_action(
		new fluid_msg::of13::SetFieldAction(
			new fluid_msg::of13::VLANPcp(pcp_tag)));
}

namespace{
	// Force versions of add_to_actions<> using write or apply
	// actions to be available during linking
	void ugly_hack() {
		fluid_msg::of13::WriteActions w;
		fluid_msg::of13::ApplyActions a;
		SwitchVLANTag t;
		t.add_to_actions(w);
		t.add_to_actions(a);
	}
}

SwitchVLANTag::SwitchVLANTag() : VLANTag::VLANTag() {
	set_type(VLANTag::switch_tag);
}

SwitchVLANTag::SwitchVLANTag(uint16_t raw) :
		VLANTag::VLANTag(raw) {
	set_type(VLANTag::switch_tag);
}

void SwitchVLANTag::set_switch(unsigned int switch_id) {
	set_value<VLANTag::num_switch_bits,1>(switch_id);
}

unsigned int SwitchVLANTag::get_switch() const {
	return get_value<VLANTag::num_switch_bits,1>();
}

PortVLANTag::PortVLANTag() : VLANTag::VLANTag() {
	set_type(VLANTag::port_tag);
}

PortVLANTag::PortVLANTag(uint16_t raw) :
		VLANTag::VLANTag(raw) {
	set_type(VLANTag::port_tag);
}

void PortVLANTag::set_slice(unsigned int slice_id) {
	set_value<VLANTag::num_slice_bits,1>(slice_id);
}

unsigned int PortVLANTag::get_slice() const {
	return get_value<VLANTag::num_slice_bits,1>();
}

void PortVLANTag::set_port(unsigned int port_id) {
	set_value<VLANTag::num_port_bits,1+VLANTag::num_slice_bits>(port_id);
}

unsigned int PortVLANTag::get_port() const {
	return get_value<VLANTag::num_port_bits,1+VLANTag::num_slice_bits>();
}

MetadataTag::MetadataTag() :
	Tag<uint64_t>::Tag() {
}

void MetadataTag::set_group(bool group_id) {
	set_value<1,0>(group_id?1:0);
}
bool MetadataTag::get_group() const {
	return get_value<1,0>();
}

void MetadataTag::set_virtual_switch(int slice_id) {
	set_value<num_virtual_switch_bits,1>(slice_id);
}
int MetadataTag::get_virtual_switch() const {
	return get_value<num_virtual_switch_bits,1>();
}

bool MetadataTag::add_to_match(fluid_msg::of13::FlowMod& flowmod) const {
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

bool MetadataTag::add_to_instruction(fluid_msg::of13::FlowMod& flowmod) const {
	// Look if there already is a write_metadata instruction
	// in the flowmod message
	fluid_msg::of13::InstructionSet instruction_set = flowmod.instructions();

	// The variables to store the existing WriteMetadata
	// information in
	uint64_t existing_tag  = 0;
	uint64_t existing_mask = 0;

	// Loop over all instructions and add them to a
	// new instruction set, unless it is a WriteMetadata
	// instruction. Then save the information in the
	// WriteMetadata instruction.
	fluid_msg::of13::InstructionSet new_instruction_set;
	for( fluid_msg::of13::Instruction* instruction : instruction_set.instruction_set() ) {
		if( instruction->type() == fluid_msg::of13::OFPIT_WRITE_METADATA ) {
			fluid_msg::of13::WriteMetadata* write_metadata =
				(fluid_msg::of13::WriteMetadata*) instruction;
			existing_tag  = write_metadata->metadata();
			existing_mask = write_metadata->metadata_mask();
		}
		else {
			new_instruction_set.add_instruction(instruction->clone());
		}
	}

	// The total amount of bits used by the hypervisor
	constexpr int total_bits = num_virtual_switch_bits + 1;

	// Check if bits in the mask are set that would be shifted out
	uint64_t mask_check
		= make_mask(total_bits) << (64-total_bits);
	if( existing_mask & mask_check ) {
		return false;
	}

	// Create the actual mask
	uint64_t new_tag  = tag  | (existing_tag <<(total_bits));
	uint64_t new_mask = mask | (existing_mask<<(total_bits));

	// Add the metadata instruction to the instruction set
	new_instruction_set.add_instruction(
		new fluid_msg::of13::WriteMetadata(
			tag,
			mask));

	// Overwrite the instructions set in the flowmod message
	flowmod.instructions(new_instruction_set);

	// Return that everything went ok
	return true;
}
