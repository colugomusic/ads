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

TEST_CASE("write examples") {
	auto data0 = ads::make(ads::channel_count{2}, ads::frame_count{10000});
	data0.write([](float* buffer, ads::frame_idx start, ads::frame_count frame_count){
		std::fill(buffer, buffer + frame_count.value, 1.0f);
		return frame_count;
	});
	auto data1 = ads::make(ads::channel_count{2}, ads::frame_count{10000});
	data1.write(ads::channel_idx{1}, [](float* buffer, ads::frame_idx start, ads::frame_count frame_count){
		std::fill(buffer, buffer + frame_count.value, 1.0f);
		return frame_count;
	});
	auto data2 = ads::make(ads::channel_count{2}, ads::frame_count{10000});
	data2.write([](float* buffer, ads::channel_idx ch, ads::frame_idx start, ads::frame_count frame_count){
		if (ch.value == 0) { std::fill(buffer, buffer + frame_count.value, 0.0f); }
		else               { std::fill(buffer, buffer + frame_count.value, 1.0f); }
		return frame_count;
	});
	auto data3 = ads::make(ads::channel_count{2}, ads::frame_count{10000});
	data3.write(ads::channel_idx{0}, [](float* buffer, ads::frame_idx start, ads::frame_count frame_count){
		std::fill(buffer, buffer + frame_count.value, 0.0f);
		return frame_count;
	});
	auto data4 = ads::make(ads::channel_count{2}, ads::frame_count{10000});
	data4.write(ads::channel_idx{1}, [](float* buffer, ads::frame_idx start, ads::frame_count frame_count){
		std::fill(buffer, buffer + frame_count.value, 1.0f);
		return frame_count;
	});
	data0.read([](const float* buffer, ads::frame_idx start, ads::frame_count frame_count){
		for (auto i = 0; i < frame_count.value; ++i) {
			CHECK (buffer[i] == doctest::Approx(1.0f));
		}
		return frame_count;
	});
	data1.read(ads::channel_idx{1}, [](const float* buffer, ads::frame_idx start, ads::frame_count frame_count){
		for (auto i = 0; i < frame_count.value; ++i) {
			CHECK (buffer[i] == doctest::Approx(1.0f));
		}
		return frame_count;
	});
	data2.read([](const float* buffer, ads::channel_idx ch, ads::frame_idx start, ads::frame_count frame_count){
		if (ch.value == 0) {
			for (auto i = 0; i < frame_count.value; ++i) {
				CHECK (buffer[i] == doctest::Approx(0.0f));
			}
		}
		else {
			for (auto i = 0; i < frame_count.value; ++i) {
				CHECK (buffer[i] == doctest::Approx(1.0f));
			}
		}
		return frame_count;
	});
	data3.read(ads::channel_idx{0}, [](const float* buffer, ads::frame_idx start, ads::frame_count frame_count){
		for (auto i = 0; i < frame_count.value; ++i) {
			CHECK (buffer[i] == doctest::Approx(0.0f));
		}
		return frame_count;
	});
	data4.read(ads::channel_idx{1}, [](const float* buffer, ads::frame_idx start, ads::frame_count frame_count){
		for (auto i = 0; i < frame_count.value; ++i) {
			CHECK (buffer[i] == doctest::Approx(1.0f));
		}
		return frame_count;
	});
}
