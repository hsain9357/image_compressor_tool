# Bulk Image Compressor

Compresses all JPEG/PNG images inside directories prefixed with `input` in the current working directory.

## Usage

```
imgcomp [quality] [mode] [options]
```

| Argument  | Default | Description |
|-----------|---------|-------------|
| `quality` | `40`    | Compression quality 0–100. Higher = better quality, larger file. |
| `mode`    | webp    | Set to `same` to keep original format; otherwise all images are transcoded to WebP. |

### Options

| Flag | Description |
|------|-------------|
| `-i, --input <path>`  | Input file or directory (overrides auto-detection) |
| `-o, --output <path>` | Output file or directory (requires `--input`) |

## How It Works (default mode)

1. Scans the current directory for folders whose name starts with `input`.
2. For each folder, creates a sibling output folder with `_com` suffix (e.g. `input_test` → `input_test_com`).
3. Compresses all `.jpg`, `.jpeg`, and `.png` images in parallel.
4. Prints a summary table with original size, compressed size, and reduction percentage.

## Format-Specific Behavior

| Input → Output | Details |
|----------------|---------|
| Any → WebP (default) | All images are converted to WebP with the given quality level. |
| JPEG → JPEG (`same`) | Recompressed with libjpeg at the given quality. |
| PNG → PNG (`same`) | Quantized to 8-bit indexed PNG via libimagequant (100% dithering). |

## Examples

```bash
# Compress all input folders to WebP at quality 40
imgcomp

# Compress to WebP at quality 75
imgcomp 75

# Keep original formats, quality 50
imgcomp 50 same

# Process a specific folder
imgcomp --input photos

# Custom input and output directories
imgcomp 75 --input photos --output compressed

# Compress a single file (output auto-named photo.webp)
imgcomp --input photo.jpg

# Single file with explicit output
imgcomp --input photo.jpg --output compressed.webp

# Quick max-quality compression
imgcomp 100 same
```

## Building

```bash
make          # build imgcomp
make clean    # remove binary
make install  # copy to /usr/local/bin
make uninstall
```

Dependencies: `libjpeg`, `libwebp`, `libpng`, `libimagequant_sys` (Rust), `g++` with C++17.
