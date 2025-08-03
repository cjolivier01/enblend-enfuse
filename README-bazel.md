# Building enblend-enfuse with Bazel

This directory contains Bazel build files for building enblend and enfuse from the source code at:
https://github.com/cjolivier01/enblend-enfuse/tree/colivier/hockeymom

## Prerequisites

1. **Install Bazel**: Follow instructions at https://bazel.build/install

2. **Install system dependencies**:
   ```bash
   # On Ubuntu/Debian:
   sudo apt-get install \
     libgsl-dev \
     libboost-all-dev \
     libtiff-dev \
     libjpeg-dev \
     libpng-dev \
     liblcms2-dev \
     libvigra-dev \
     libexiv2-dev \
     libopenexr-dev
   
   # On macOS with MacPorts:
   sudo port install \
     gsl \
     boost \
     tiff \
     jpeg \
     libpng \
     lcms2 \
     vigra \
     exiv2 \
     openexr
   ```

## Configuration

1. **Adjust library paths**: Edit the `WORKSPACE` file to point to your system library locations. The default assumes libraries are in `/usr`.

2. **Configure features**: Edit `config.h` generation in the root `BUILD` file to enable/disable features:
   - `HAVE_LIBEXIV2`: Enable EXIF metadata support
   - `HAVE_OPENEXR`: Enable OpenEXR format support
   - `HAVE_OPENCL`: Enable OpenCL acceleration
   - `OPENMP`: Enable OpenMP parallelization

## Building

### Basic build:
```bash
# Build enblend
bazel build //:enblend

# Build enfuse
bazel build //:enfuse

# Build both
bazel build //:enblend //:enfuse
```

### Build with optional features:
```bash
# With OpenEXR support
bazel build --config=with_openexr //:enblend //:enfuse

# With EXIF metadata support
bazel build --config=with_exiv2 //:enblend //:enfuse

# With OpenMP parallelization
bazel build --config=openmp //:enblend //:enfuse

# Debug build
bazel build --config=debug //:enblend //:enfuse
```

### Platform-specific builds:
```bash
# Linux
bazel build --config=linux //:enblend //:enfuse

# macOS
bazel build --config=macos //:enblend //:enfuse

# Windows
bazel build --config=windows //:enblend //:enfuse
```

## Running

After building, the executables will be located at:
- `bazel-bin/src/enblend`
- `bazel-bin/src/enfuse`

Example usage:
```bash
# Run enblend
bazel-bin/src/enblend -o output.tif input1.tif input2.tif

# Run enfuse
bazel-bin/src/enfuse -o output.tif input1.tif input2.tif input3.tif
```

## Testing

```bash
# Run all tests
bazel test //...

# Run specific test targets (when available)
bazel test //src:enblend_test
```

## Cleaning

```bash
# Clean build artifacts
bazel clean

# Clean everything including external dependencies
bazel clean --expunge
```

## Troubleshooting

1. **Missing headers**: If you get "file not found" errors for headers, check that:
   - The library is installed on your system
   - The path in `WORKSPACE` is correct
   - The include path in the `build_file_content` is correct

2. **Linking errors**: If you get undefined symbol errors:
   - Check that the `.so` or `.a` files exist in the specified library paths
   - Ensure all required libraries are listed in `deps`

3. **OpenCL issues**: If building with OpenCL support:
   - Ensure OpenCL headers and libraries are installed
   - Update the OpenCL kernel path in `config.h`

4. **Build too slow**: Use these flags to speed up builds:
   ```bash
   bazel build --jobs=HOST_CPUS*0.5 //:enblend
   ```

## File Structure

```
enblend-enfuse/
├── WORKSPACE           # External dependencies
├── BUILD               # Root build configuration
├── BUILD.bazel         # Feature configurations
├── .bazelrc           # Bazel configuration
└── src/
    ├── BUILD          # Main source build rules
    ├── layer_selection/
    │   └── BUILD      # Layer selection library
    └── dynamic_loader/
        └── BUILD      # Dynamic loader library
```

## Notes

- This Bazel build is based on the CMake and Autotools configurations in the original project
- Some features may require additional configuration depending on your system
- The build assumes C++11 support (as required by the original project)
- OpenCL kernel files (`.cl`) need to be converted to `.icl` includes using the `embrace` tool
