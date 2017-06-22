# We are using C++11
set(CMAKE_CXX_STANDARD 11)

add_executable(delftvisor
	main.cpp
	hypervisor.cpp
	slice.cpp
	virtual_switch.cpp
	virtual_switch_unused.cpp
	physical_switch.cpp
	physical_switch_unused.cpp
	physical_switch_topology.cpp
	physical_switch_flowtable.cpp
	physical_switch_rewrite.cpp
	openflow_connection.cpp
	discoveredlink.cpp
	tag.cpp)

include_directories(${LibFluid_INCLUDE_DIRS})
target_link_libraries(delftvisor ${LibFluid_LIBRARIES})

# Find and link with boost
find_package(Boost
	1.58.0
	REQUIRED
	system program_options thread log
)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(delftvisor ${Boost_LIBRARIES})

# Needed to get boost log to compile
add_definitions(-DBOOST_LOG_DYN_LINK -DBOOST_USE_VALGRIND -g)
