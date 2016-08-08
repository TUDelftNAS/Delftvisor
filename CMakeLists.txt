cmake_minimum_required(VERSION 3.5.2)
project(Hypervisor)

# Get libfluid to build
include(ExternalProject)
ExternalProject_Add(libfluid_msg
	LIST_SEPARATOR    ","
	PREFIX            "libfluid"
	GIT_REPOSITORY    "git@github.com:OpenNetworkingFoundation/libfluid_msg.git"
	CONFIGURE_COMMAND ../libfluid_msg/autogen.sh && ../libfluid_msg/configure --prefix=${CMAKE_BINARY_DIR}/libfluid_build
	UPDATE_COMMAND    ""
	LOG_DOWNLOAD      ON
	LOG_UPDATE        ON
	LOG_CONFIGURE     ON
	LOG_BUILD         ON
	LOG_INSTALL       ON
)
set(LibFluid_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/libfluid_build/include)
set(LibFluid_LIBRARIES ${CMAKE_BINARY_DIR}/libfluid_build/lib/libfluid_msg.so)

add_subdirectory(src)
add_subdirectory(tests)
