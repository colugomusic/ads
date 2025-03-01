# ads
This is a header-only c++20 library for storing audio data in memory.

## Motivation

If you write audio code you have probably written a data structure that looks something like this at some point:
```c++
template <size_t channel_count>
struct audio_data {
  std::array<std::vector<float>, channel_count> frames;
};
```
If you are like me then you have probably written multiple variations of this. There are four obvious variations depending on which dimensions of the buffer are known at compile time:

`std::array<std::vector<float>, channel_count>`
- Channel count known at compile time, but dynamic number of frames (as above.)

`std::vector<std::array<float, frame_count>>`
- Frame count known at compile time, but dynamic number of channels.

`std::array<std::array<float, frame_count>, channel_count>`
- Both channel count and frame count known at compile time.

`std::vector<std::vector<float>>`
- Dynamic number of channels and frames.

This library consolidates all these variations into one consistent interface, and provides utilities for reading and writing the data, iterating over multi-channel data frame-by-frame, and interleaving operations.

## Requirements

- c++20 or above
- This library make use of `aligned_allocator` and `small_vector` from [Boost](https://www.boost.org/) so you just need to make sure these are available in your include paths:
```c++
#include <boost/align/aligned_allocator.hpp>
#include <boost/container/small_vector.hpp>
```
If using CMake then this will happen automatically as long as `find_package(Boost REQUIRED COMPONENTS headers CONFIG)` succeeds.

## CMake
After cloning you can use the library in your CMake project like this:
```cmake
add_subdirectory("/path/to/ads")
target_link_libraries(your-project ads::ads)
```

Or if not using CMake, simply copy+paste the headers into your project.

## Extra configuration
You can override `ADS_ASSERT` before including `ads.hpp` to have the library use a different assertion macro:
```c++
#define ADS_ASSERT(x) MY_ASSERT(x)
#include <ads.hpp>
```
By default `<cassert>` is used.

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

// 10,000 frames of interleaved stereo data (therefore the underlying buffer is a single channel of 20,000 frames.)
auto interleaved = ads::interleaved{ads::channel_count{2}, ads::frame_count{10000}};

// Convert from interleaved to multi-channel
ads::deinterleave(interleaved, data3.begin());

// Convert from multi-channel to interleaved
ads::interleave(data3, interleaved.begin());

// You can also just use any old range of floats for interleaved data
auto buffer = std::vector<float>(20000, 0.0f);
ads::interleave(data3, buffer.begin());
ads::deinterleave(buffer, data3.begin());
```
Although their types are different, the same interface (more or less) is provided for the `data0`, `data1`, `data2` and `data3` objects created above. There are some extra things enabled if the storage is known at compile-time to be mono-channel.
- `get_channel_count()`
- `get_frame_count()`
- `begin()`/`end()` : returns specialized iterators for iterating frame-by-frame (even though channels are stored in separate buffers)
- `resize()` : resize the storage (channel count or frame count, or both)
- `set()` : set individual frame values
- `at()` : return individual frames (by reference), or underlying channel buffers
- `write()` : for writing audio data to the storage
- `read()` : for reading audio data from the storage
- `data()` : access the raw `float*` buffers

## Reading and writing audio data

The `read()` and `write()` functions are based around the idea of reading and writing chunks of the underlying storage buffers, since this is usually what you want to do in audio code, rather than iterating frame-by-frame.

You can pass in either a single-channel or multi-channel read/write function.

A "single-channel" read function has the form:

`(const float* buffer, ads::frame_idx start, ads::frame_count frame_count) -> ads::frame_count`

A "multi-channel" read function has the form:

`(const float* buffer, ads::channel_idx ch, ads::frame_idx start, ads::frame_count frame_count) -> ads::frame_count`

A "single-channel" write function has the form:

`(float* buffer, ads::frame_idx start, ads::frame_count frame_count) -> ads::frame_count`

A "multi-channel" write function has the form:

`(float* buffer, ads::channel_idx ch, ads::frame_idx start, ads::frame_count frame_count) -> ads::frame_count`

- `buffer` is always pre-offset into the part of the underlying buffer that you are reading from or writing to.
- `ch` is the index of the channel that you are reading from or writing to.
- `start` is the index of the first frame of the chunk of frames you are reading from or writing to. This is often not needed but is useful in some situations.
- `frame_count` is the maximum number of frames you should try to read or write. This value will never overflow the end of the underlying buffer.
- The actual number of frames read or written should be returned from the function.

### Writing examples

This will write ones to both channels:
```c++
auto data = ads::make(ads::channel_count{2}, ads::frame_count{10000});
data.write([](float* buffer, ads::frame_idx start, ads::frame_count frame_count){
  std::fill(buffer, buffer + frame_count.value, 1.0f);
  return frame_count;
});
```

This will write ones to only the second channel:
```c++
auto data = ads::make(ads::channel_count{2}, ads::frame_count{10000});
data.write(ads::channel_idx{1}, [](float* buffer, ads::frame_idx start, ads::frame_count frame_count){
  std::fill(buffer, buffer + frame_count.value, 1.0f);
  return frame_count;
});
```

This will write zeros to the first channel, and ones to the second channel:
```c++
auto data = ads::make(ads::channel_count{2}, ads::frame_count{10000});
data.write([](float* buffer, ads::channel_idx ch, ads::frame_idx start, ads::frame_count frame_count){
  if (ch.value == 0) { std::fill(buffer, buffer + frame_count.value, 0.0f); }
  else               { std::fill(buffer, buffer + frame_count.value, 1.0f); }
  return frame_count;
});
```

This does the same thing:
```c++
auto data = ads::make(ads::channel_count{2}, ads::frame_count{10000});
data.write(ads::channel_idx{0}, [](float* buffer, ads::frame_idx start, ads::frame_count frame_count){
  std::fill(buffer, buffer + frame_count.value, 0.0f);
  return frame_count;
});
data.write(ads::channel_idx{1}, [](float* buffer, ads::frame_idx start, ads::frame_count frame_count){
  std::fill(buffer, buffer + frame_count.value, 1.0f);
  return frame_count;
});
```

## Madronalib extension
If you happen to use [Madronalib](https://github.com/madronalabs/madronalib) in your project there is [an extra header](include/ads/ads-ml.hpp) with some utilities for interacting with `ml::DSPVector`, `ml::DSPVectorArray`, and `ml::DSPVectorDynamic`:
```c++
#include <ads-ml.hpp>
```
