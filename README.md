# ads
This is a header-only c++20 library for in-memory audio data storage.

## CMake
After cloning you can use the library in your CMake project like this:
```cmake
add_subdirectory("/path/to/ads")
target_link_libraries(your-project ads::ads)
```

Or if not using CMake, simply copy+paste the headers into your project.

## Usage
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
If you use [Madronalib](https://github.com/madronalabs/madronalib) in your project there is an extra header with some extension functions:
```c++
#include <ads-ml.hpp>
```
