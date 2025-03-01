cmake_minimum_required(VERSION 3.23)
project(ads-test)
list(APPEND ads-test-src
	src/doctest.h
	src/main.cpp
)
add_executable(ads-test ${ads-test-src})
if (NOT TARGET ads::ads)
	set (ADS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	add_subdirectory(../.. ads)
endif()
target_link_libraries(ads-test ads::ads)
