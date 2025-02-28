cmake_minimum_required(VERSION 3.23)
project(ads)
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