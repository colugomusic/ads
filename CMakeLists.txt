cmake_minimum_required(VERSION 3.23)
project(ads)
option(ADS_BUILD_TESTS "Build ADS tests" OFF)
add_library(ads INTERFACE)
add_library(ads::ads ALIAS ads)
target_sources(ads INTERFACE
	FILE_SET HEADERS
	BASE_DIRS include/ads
	FILES
		include/ads/ads.hpp
		include/ads/ads-concepts-basic.hpp
		include/ads/ads-concepts-fns.hpp
		include/ads/ads-mipmap.hpp
		include/ads/ads-ml.hpp
		include/ads/ads-vocab.hpp
)
find_package(Boost REQUIRED COMPONENTS align container)
target_link_libraries(ads INTERFACE Boost::align Boost::container)
target_compile_features(ads INTERFACE cxx_std_20)
install(TARGETS ads FILE_SET HEADERS)
if (ADS_BUILD_TESTS)
	add_subdirectory(test)
endif()