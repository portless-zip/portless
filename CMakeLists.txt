cmake_minimum_required(VERSION 3.15)
project(portless C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/headers/common)

if(MSVC)
	set(COMMON_C_FLAGS /W4 /O2)
else()
	set(COMMON_C_FLAGS -Wall -Wextra -O2)
endif()

function(portless_target name source)
	add_executable(${name} ${source})
	target_include_directories(${name} PRIVATE ${COMMON_INCLUDE_DIR})
	target_compile_options(${name} PRIVATE ${COMMON_C_FLAGS})

	if(WIN32)
		target_link_libraries(${name} ws2_32)
	endif()
endfunction()

portless_target(client clients/client0/client.c)
portless_target(relay  relays/relay0/relay.c)

if(WIN32)
	portless_target(client_win clients/client0/client.c)
	portless_target(relay_win  relays/relay0/relay.c)
endif()
