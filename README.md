# ads
This is a header-only c++20 library for storing audio data in memory.

If you write audio code you have probably written a data structure that looks something like this at some point:
```c++
template <size_t channel_count>
struct audio_data {
  std::array<std::vector<float>, channel_count> frames;
};
```
If you are like me then you have probably written multiple variations of this. There are four obvious variations depending on which dimensions of the buffer are known at compile time:
- Channel count known at compile time, but dynamic number of frames (as above): `std::array<std::vector<float>, channel_count>`
- Frame count known at compile time, but dynamic number of channels: `std::vector<std::array<float, frame_count>>`
- Both channel count and frame count known at compile time: `std::array<std::array<float, frame_count>, channel_count>`
- Dynamic number of channels and frames: `std::vector<std::vector<float>>`

This library consolidates all these variations into one consistent interface, and provides utilities for reading and writing the data, iterating over multi-channel data frame-by-frame, and interleaving operations.

## CMake
After cloning you can use the library in your CMake project like this:
```cmake
add_subdirectory("/path/to/ads")
target_link_libraries(your-project ads::ads)
```

Or if not using CMake, simply copy+paste the headers into your project.

## Types

*`template <uint64_t channel_count, uint64_t frame_count>`* *`ads::data`*
- Main audio storage type. The underlying storage type depends on the template arguments.
- `ads::DYNAMIC_EXTENT` can be used for either `channel_count` or `frame_count`, or both.
- `ads::DYNAMIC_EXTENT` means the count can be specified at runtime (and the `resize()` function will be available for that dimension.)

*`template <uint64_t frame_count>`* *`ads::mono`*
- 1 channel of a compile-time-known number of frames (unless `DYNAMIC_EXTENT` is specified.)
- An alias for `ads::data<1, frame_count>`.

*`template <uint64_t frame_count>`* *`ads::stereo`*
- 2 channels of a compile-time-known number of frames (unless `DYNAMIC_EXTENT` is specified.)
- An alias for `ads::data<2, frame_count>`.

*`ads::dynamic_mono`*
- 1 channel of a dynamic number of frames.
- An alias for `ads::data<1, ads::DYNAMIC_EXTENT>`.

*`ads::dynamic_stereo`*
- 2 channels of a dynamic number of frames.
- An alias for `ads::data<2, ads::DYNAMIC_EXTENT>`.

*`ads::fully_dynamic`*
- A dynamic number of channels and frames.
- An alias for `ads::data<ads::DYNAMIC_EXTENT, ads::DYNAMIC_EXTENT>`.

*`ads::interleaved`*
- A wrapper around `ads::dynamic_mono` intended to be used for interleaved audio channel data.
- The channel count and frame count are specified at runtime and the total number of required underlying frames is calculated for you.

## Usage Examples
```c++
// Mono data
// Frame count known at runtime
// type == ads::dynamic_mono / ads::data<1, ads::DYNAMIC_EXTENT>
auto mono_data0 = ads::make_mono(ads::frame_count{10000});
auto mono_data1 = ads::make<1>(ads::frame_count{10000}); // equivalent

// Stereo data
// Frame count known at runtime
// type == ads::dynamic_stereo / ads::data<2, ads::DYNAMIC_EXTENT>
auto stereo_data0 = ads::make_stereo(ads::frame_count{10000});
auto stereo_data1 = ads::make<2>(ads::frame_count{10000}); // equivalent

// Arbitrary number of channels known at compile time
// Frame count known at runtime
// type == ads::data<10, ads::DYNAMIC_EXTENT>
auto data0 = ads::make<10>(ads::frame_count{10000});

// Frame count known at compile time
// Channel count known at runtime
// type == ads::data<ads::DYNAMIC_EXTENT, 10>
auto data1 = ads::make<10>(ads::channel_count{2});

// Channel count and frame count both known at compile time
// type == ads::data<2, 64>
auto data2 = ads::make<2, 64>();

// Channel count and frame count both known at runtime
// type == ads::fully_dynamic / ads::data<DYNAMIC_EXTENT, DYNAMIC_EXTENT>
auto data3 = ads::make(ads::channel_count{2}, ads::frame_count{10000});

// Single-channel buffer intended for interleaved audio data
auto interleaved = ads::interleaved{ads::channel_count{2}, ads::frame_count{10000});

// Convert from interleaved to multi-channel
ads::deinterleave(interleaved, data3.begin());

// Convert from multi-channel to interleaved
ads::interleave(data3, interleaved.begin());

// You can also just use any old range of floats for interleaved data
auto buffer = std::vector<float>{20000};
ads::interleave(data3, buffer.begin());
ads::deinterleave(buffer, data3.begin());
```
Although their types are different, the same interface (more or less) is provided for the `data0`, `data1`, `data2` and `data3` objects created above. There are some extra things enabled if the storage is known at compile-time to be mono-channel.
- `get_channel_count()`
- `get_frame_count()`
- `begin()`/`end()` : returns specialized iterators for iterating frame-by-frame (even though channels are stored in separate buffers)
- `resize()` : resize the storage (channel count or frame count)
- `set()` : set frame values
- `at()` : return frames, or underlying channel buffers
- `write()` : for writing audio data to the storage
- `read()` : for reading audio data from the storage

## Madronalib extension
If you happen to use [Madronalib](https://github.com/madronalabs/madronalib) in your project there is [an extra header](include/ads/ads-ml.hpp) with some extension functions:
```c++
#include <ads-ml.hpp>
```
