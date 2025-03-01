#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <numeric>
#include "ads.hpp"
#include "doctest.h"

TEST_CASE("mono static tests") {
	auto st = ads::make_mono<512>();
	static_assert (st.get_channel_count() == ads::channel_count{1});
	static_assert (st.get_frame_count() == ads::frame_count{512});
}

TEST_CASE("mono dynamic write/read") {
	auto st = ads::make_mono({512});
	REQUIRE (st.get_channel_count() == ads::channel_count{1});
	REQUIRE (st.get_frame_count() == ads::frame_count{512});
	st.write(ads::frame_count{512}, [](float* buffer, ads::frame_idx start, ads::frame_count frame_count) {
		std::iota(buffer, buffer + frame_count.value, 0.0f);
		return ads::frame_count{frame_count.value};
	});
	st.read(ads::frame_count{512}, [](const float* buffer, ads::frame_idx start, ads::frame_count frame_count) {
		for (auto i = 0; i < frame_count.value; ++i) {
			CHECK (buffer[i] == doctest::Approx(i));
		}
		return ads::frame_count{frame_count.value};
	});
}
