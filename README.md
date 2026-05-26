# Bulk Image Compressor

![logo](logo.svg)

A fast, multi-threaded C++17 tool for bulk compressing JPEG and PNG images. Supports transcoding to WebP or keeping the original format with optimized compression.

## Features

- **Bulk processing** — scans all `input*` directories and compresses images in parallel
- **WebP conversion** — default mode transcodes all images to WebP
- **Same-format mode** — recompress JPEG via libjpeg, quantize PNG to 8-bit indexed via libimagequant
- **Single file mode** — compress individual images with `--input`
- **Custom paths** — specify arbitrary input/output directories or files

## Prerequisites

- `g++` with C++17 support
- `cmake`, `make`, `curl` (for building static dependencies)
- Rust/Cargo (for the `libimagequant` submodule)

## Quick Start

```bash
# Clone with submodules
git clone --recurse-submodules <repo-url>
cd bulk_image_compressor

# Build the tool (static binary, no system libs needed)
make

# Run it
./imgcomp
```

The first `make` run downloads and builds `zlib`, `libjpeg-turbo`, `libpng`, and `libwebp` from source as static libraries — no development packages required. Subsequent builds reuse the cached static libs.

See [USAGE.md](USAGE.md) for detailed usage and examples.

## Install

### From source
```bash
make install   # copies to /usr/local/bin
```

### Pre-built binary (no build tools needed)
```bash
curl -L https://github.com/hsain9357/image_compressor_tool/releases/download/v1.0.0/imgcomp -o /usr/local/bin/imgcomp && chmod +x /usr/local/bin/imgcomp
```

## How It Works

The tool scans the current directory for folders starting with `input`, creates sibling output folders with a `_com` suffix, and compresses all `.jpg`, `.jpeg`, and `.png` images inside them concurrently using `std::async`.

- **Default mode** — every image is loaded as RGBA and encoded as WebP
- **`same` mode** — JPEGs stay JPEG (libjpeg), PNGs stay PNG (quantized to 8-bit palette via libimagequant)

## Project Structure

| Path | Description |
|------|-------------|
| `src/main.cpp` | Single source file with the entire program |
| `include/stb_image.h` | Image loader (header-only) |
| `include/stb_image_write.h` | Image writer (header-only) |
| `libimagequant/` | Git submodule — Rust library for PNG quantization |
| `Makefile` | Build system |
