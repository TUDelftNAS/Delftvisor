#pragma once

/// A generic class to store a bidirectional map between virtual and physical
template<typename VirtualType, typename PhysicalType>
class bidirectional_map {
	std::unordered_map<VirtualType,PhysicalType> virtual_to_physical;
	std::unordered_map<PhysicalType,VirtualType> physical_to_virtual;

public:
	void insert(VirtualType virtual_var, PhysicalType physical_var) {
		virtual_to_physical[virtual_var]  = physical_var;
		physical_to_virtual[physical_var] = virtual_var;
	}

	void erase(VirtualType virtual_var) {
		auto it = virtual_to_physical.find(virtual_var);
		physical_to_virtual.erase(it->second);
		virtual_to_physical.erase(it);
	}

	size_t size() const {
		return virtual_to_physical.size();
	}

	bool has_virtual(VirtualType virtual_var) const {
		return virtual_to_physical.find(virtual_var) != virtual_to_physical.end();
	}
	bool has_physical(PhysicalType physical_var) const {
		return physical_to_virtual.find(physical_var) != physical_to_virtual.end();
	}

	VirtualType get_virtual(PhysicalType physical_var) const {
		return physical_to_virtual.at(physical_var);
	}
	PhysicalType get_physical(VirtualType virtual_var) const {
		return virtual_to_physical.at(virtual_var);
	}

	const std::unordered_map<VirtualType,PhysicalType>& get_virtual_to_physical() const {
		return virtual_to_physical;
	}
	const std::unordered_map<PhysicalType,VirtualType>& get_physical_to_virtual() const {
		return physical_to_virtual;
	}
};
