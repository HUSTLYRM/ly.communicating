cmake_minimum_required(VERSION 3.21)

project(ly.communicating.core)

add_library(ly_communicating_core INTERFACE)
add_library(ly::communicating::core ALIAS ly_communicating_core)
target_include_directories(ly_communicating_core INTERFACE include)
set_target_properties(ly_communicating_core PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
)

if (PROJECT_IS_TOP_LEVEL)
	add_executable(ly_communicating_core_use_interface test/use_interface.cpp)
	target_link_libraries(ly_communicating_core_use_interface PRIVATE ly::communicating::core)
	set_target_properties(ly_communicating_core_use_interface PROPERTIES
	add_executable(ly_communicating_core_test test/test.cpp)
	target_link_libraries(ly_communicating_core_test PRIVATE ly::communicating::core)
	set_target_properties(ly_communicating_core_test PROPERTIES
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
	)
endif ()
