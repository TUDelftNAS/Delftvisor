# We are using C++11
set(CMAKE_CXX_STANDARD 11)

add_executable(hypervisor
	main.cpp
	hypervisor.cpp
	slice.cpp
	virtual_switch.cpp
	physical_switch.cpp
	openflow_connection.cpp)

include_directories(${LibFluid_INCLUDE_DIRS})
target_link_libraries(hypervisor ${LibFluid_LIBRARIES})

# Find and link with boost
find_package(Boost
	1.58.0
	REQUIRED
	system program_options thread
)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(hypervisor ${Boost_LIBRARIES})
