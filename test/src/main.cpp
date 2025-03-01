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

TEST_CASE("usage examples") {
	// Mono data
	// Frame count known at runtime
	// type == ads::dynamic_mono / ads::data<1, ads::DYNAMIC_EXTENT>
	auto mono_data0 = ads::make_mono(ads::frame_count{10000});
	auto mono_data1 = ads::make<1>(ads::frame_count{10000}); // equivalent
	static_assert (mono_data0.get_channel_count() == ads::channel_count{1});
	static_assert (mono_data1.get_channel_count() == ads::channel_count{1});
	REQUIRE (mono_data0.get_channel_count() == ads::channel_count{1});
	REQUIRE (mono_data0.get_frame_count() == ads::frame_count{10000});
	REQUIRE (mono_data1.get_channel_count() == ads::channel_count{1});
	REQUIRE (mono_data1.get_frame_count() == ads::frame_count{10000});

	// Stereo data
	// Frame count known at runtime
	// type == ads::dynamic_stereo / ads::data<2, ads::DYNAMIC_EXTENT>
	auto stereo_data0 = ads::make_stereo(ads::frame_count{10000});
	auto stereo_data1 = ads::make<2>(ads::frame_count{10000}); // equivalent
	static_assert (stereo_data0.get_channel_count() == ads::channel_count{2});
	static_assert (stereo_data1.get_channel_count() == ads::channel_count{2});
	REQUIRE (stereo_data0.get_channel_count() == ads::channel_count{2});
	REQUIRE (stereo_data0.get_frame_count() == ads::frame_count{10000});
	REQUIRE (stereo_data1.get_channel_count() == ads::channel_count{2});
	REQUIRE (stereo_data1.get_frame_count() == ads::frame_count{10000});

	// Arbitrary number of channels known at compile time
	// Frame count known at runtime
	// type == ads::data<10, ads::DYNAMIC_EXTENT>
	auto data0 = ads::make<10>(ads::frame_count{10000});
	static_assert (data0.get_channel_count() == ads::channel_count{10});
	REQUIRE (data0.get_channel_count() == ads::channel_count{10});
	REQUIRE (data0.get_frame_count() == ads::frame_count{10000});

	// Frame count known at compile time
	// Channel count known at runtime
	// type == ads::data<ads::DYNAMIC_EXTENT, 10>
	auto data1 = ads::make<10>(ads::channel_count{2});
	static_assert (data1.get_frame_count() == ads::frame_count{10});
	REQUIRE (data1.get_channel_count() == ads::channel_count{2});
	REQUIRE (data1.get_frame_count() == ads::frame_count{10});

	// Channel count and frame count both known at compile time
	// type == ads::data<2, 64>
	auto data2 = ads::make<2, 64>();
	static_assert (data2.get_channel_count() == ads::channel_count{2});
	static_assert (data2.get_frame_count() == ads::frame_count{64});

	// Channel count and frame count both known at runtime
	// type == ads::fully_dynamic / ads::data<DYNAMIC_EXTENT, DYNAMIC_EXTENT>
	auto data3 = ads::make(ads::channel_count{2}, ads::frame_count{10000});
	REQUIRE (data3.get_channel_count() == ads::channel_count{2});
	REQUIRE (data3.get_frame_count() == ads::frame_count{10000});

	// 10,000 frames of interleaved stereo data (therefore the underlying buffer is a single channel of 20,000 frames.)
	auto interleaved = ads::interleaved{ads::channel_count{2}, ads::frame_count{10000}};

	interleaved.at(0) = 0.0f;
	interleaved.at(1) = 0.0f;
	interleaved.at(2) = 1.0f;
	interleaved.at(3) = 1.0f;
	interleaved.at(4) = 2.0f;
	interleaved.at(5) = 2.0f;

	// Convert from interleaved to multi-channel
	ads::deinterleave(interleaved, data3.begin());

	REQUIRE (data3.at(ads::channel_idx{0}, ads::frame_idx{0}) == 0.0f);
	REQUIRE (data3.at(ads::channel_idx{1}, ads::frame_idx{0}) == 0.0f);
	REQUIRE (data3.at(ads::channel_idx{0}, ads::frame_idx{1}) == 1.0f);
	REQUIRE (data3.at(ads::channel_idx{1}, ads::frame_idx{1}) == 1.0f);
	REQUIRE (data3.at(ads::channel_idx{0}, ads::frame_idx{2}) == 2.0f);
	REQUIRE (data3.at(ads::channel_idx{1}, ads::frame_idx{2}) == 2.0f);

	// Convert from multi-channel to interleaved
	ads::interleave(data3, interleaved.begin());

	// You can also just use any old range of floats for interleaved data
	auto buffer = std::vector<float>(20000, 0.0f);
	ads::interleave(data3, buffer.begin());
	ads::deinterleave(buffer, data3.begin());
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
