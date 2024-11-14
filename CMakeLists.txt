cmake_minimum_required(VERSION 3.21)

project(ly.communicating)

add_subdirectory(module/core)

if (PROJECT_IS_TOP_LEVEL)
	add_executable(ly_communicating_test test/test.cpp)
	target_link_libraries(ly_communicating_test PRIVATE ly::communicating::core)
	set_target_properties(ly_communicating_test PROPERTIES
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
	)
endif ()

