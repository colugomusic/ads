#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <numeric>
#include "ads.hpp"
#include "doctest.h"

template <uint64_t Chs, uint64_t Frs>
auto write_read_iota(ads::data<Chs, Frs>* st, ads::channel_idx ch) -> void {
	ads::frame_count count;
	count = st->write(ch, [](float* buffer, ads::frame_idx start, ads::frame_count frame_count) {
		std::iota(buffer, buffer + frame_count.value, 0.0f);
		return frame_count;
	});
	REQUIRE (count == st->get_frame_count());
	count = st->read(ch, [](const float* buffer, ads::frame_idx start, ads::frame_count frame_count) {
		for (auto i = 0; i < frame_count.value; ++i) {
			CHECK (buffer[i] == doctest::Approx(i));
		}
		return frame_count;
	});
	REQUIRE (count == st->get_frame_count());
}

TEST_CASE("mono static") {
	auto st = ads::make_mono<512>();
	static_assert (st.get_channel_count() == ads::channel_count{1});
	static_assert (st.get_frame_count() == ads::frame_count{512});
}

TEST_CASE("mono dynamic") {
	auto st = ads::make_mono({512});
	REQUIRE (st.get_channel_count() == ads::channel_count{1});
	REQUIRE (st.get_frame_count() == ads::frame_count{512});
	write_read_iota(&st, ads::channel_idx{0});
}
