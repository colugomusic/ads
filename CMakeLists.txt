cmake_minimum_required(VERSION 3.23)
project(ads)
option(ADS_BUILD_TESTS "Build ADS tests" OFF)
add_library(ads INTERFACE)
add_library(ads::ads ALIAS ads)
target_sources(ads INTERFACE
	FILE_SET ads
	TYPE HEADERS
	BASE_DIRS include/ads
	FILES
		include/ads/ads.hpp
		include/ads/ads-ml.hpp
)
find_package(Boost REQUIRED COMPONENTS headers CONFIG)
target_link_libraries(ads INTERFACE Boost::headers)
target_compile_features(ads INTERFACE cxx_std_20)
if (ADS_BUILD_TESTS)
	add_subdirectory(test)
endif()