#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <jpeglib.h>
#include <webp/encode.h>
#include <libimagequant.h>
#include <png.h>
#include <stdio.h>
#include <iomanip>
#include <fstream>
#include <thread>
#include <mutex>
#include <future>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace fs = std::filesystem;

std::mutex cout_mutex;


std::string format_size(uintmax_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << (double)bytes / (1024 * 1024) << " MB";
    return ss.str();
}

bool compress_jpeg(const fs::path& output_path, int quality, int width, int height, unsigned char* img) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE* outfile;
    JSAMPROW row_pointer[1];
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    if ((outfile = fopen(output_path.c_str(), "wb")) == NULL) return false;
    jpeg_stdio_dest(&cinfo, outfile);
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    int row_stride = width * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &img[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
    return true;
}

bool write_indexed_png(const char* filename, int width, int height, unsigned char* indices, const liq_palette* pal) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) return false;

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) { fclose(fp); return false; }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) { png_destroy_write_struct(&png_ptr, NULL); fclose(fp); return false; }

    if (setjmp(png_jmpbuf(png_ptr))) { png_destroy_write_struct(&png_ptr, &info_ptr); fclose(fp); return false; }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_PALETTE,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_color palette[256];
    png_byte trans[256];
    int num_trans = 0;
    for (int i = 0; i < pal->count; i++) {
        palette[i].red = pal->entries[i].r;
        palette[i].green = pal->entries[i].g;
        palette[i].blue = pal->entries[i].b;
        trans[i] = pal->entries[i].a;
        if (pal->entries[i].a < 255) num_trans = i + 1;
    }

    png_set_PLTE(png_ptr, info_ptr, palette, pal->count);
    if (num_trans > 0) png_set_tRNS(png_ptr, info_ptr, trans, num_trans, NULL);

    png_write_info(png_ptr, info_ptr);
    for (int y = 0; y < height; y++) {
        png_write_row(png_ptr, &indices[y * width]);
    }
    png_write_end(png_ptr, info_ptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    return true;
}

bool compress_png(const fs::path& output_path, int quality, int width, int height, unsigned char* img) {
    liq_attr *attr = liq_attr_create();
    liq_set_quality(attr, 0, quality);
    liq_image *liq_img = liq_image_create_rgba(attr, img, width, height, 0);
    liq_result *res;
    bool success = false;
    
    if (liq_image_quantize(liq_img, attr, &res) == LIQ_OK) {
        liq_set_dithering_level(res, 1.0);
        unsigned char *indices = (unsigned char *)malloc(width * height);
        const liq_palette *pal = liq_get_palette(res);
        liq_write_remapped_image(res, liq_img, indices, width * height);
        
        success = write_indexed_png(output_path.c_str(), width, height, indices, pal);
        
        free(indices);
        liq_result_destroy(res);
    }
    liq_image_destroy(liq_img);
    liq_attr_destroy(attr);
    return success;
}

bool compress_webp(const fs::path& output_path, int quality, int width, int height, unsigned char* img, int channels) {
    uint8_t* output;
    size_t size;
    if (channels == 3) {
        size = WebPEncodeRGB(img, width, height, width * 3, (float)quality, &output);
    } else {
        size = WebPEncodeRGBA(img, width, height, width * 4, (float)quality, &output);
    }
    if (size == 0) return false;
    std::ofstream out(output_path, std::ios::binary);
    out.write((char*)output, size);
    WebPFree(output);
    return out.good();
}

bool compress_image(const fs::path& input_path, const fs::path& output_path, int quality, bool same_format) {
    int width, height, channels;
    std::string ext = output_path.extension().string();
    for (auto& c : ext) c = std::tolower(c);

    if (!same_format) {
        unsigned char* img = stbi_load(input_path.c_str(), &width, &height, &channels, 4);
        if (!img) return false;
        bool success = compress_webp(output_path, quality, width, height, img, 4);
        stbi_image_free(img);
        return success;
    } else {
        if (ext == ".jpg" || ext == ".jpeg") {
            unsigned char* img = stbi_load(input_path.c_str(), &width, &height, &channels, 3);
            if (!img) return false;
            bool success = compress_jpeg(output_path, quality, width, height, img);
            stbi_image_free(img);
            return success;
        } else if (ext == ".png") {
            unsigned char* img = stbi_load(input_path.c_str(), &width, &height, &channels, 4);
            if (!img) return false;
            bool success = compress_png(output_path, quality, width, height, img);
            stbi_image_free(img);
            return success;
        }
    }
    return false;
}

void process_directory(const fs::path& input_dir, int quality, bool same_format, const fs::path& output_dir_override = "") {
    fs::path output_dir = output_dir_override.empty()
        ? input_dir.parent_path() / (input_dir.filename().string() + "_com")
        : output_dir_override;
    if (!fs::exists(output_dir)) fs::create_directories(output_dir);

    std::cout << "\033[1;33mProcessing: " << input_dir.filename() << " (" << quality << "% quality, format: " << (same_format ? "original" : "webp") << ")\033[0m" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << std::left << std::setw(30) << "File" << std::setw(15) << "Original" << std::setw(15) << "Compressed" << "Reduction" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            for (auto& c : ext) c = std::tolower(c);
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
                files.push_back(entry.path());
            }
        }
    }

    std::vector<std::future<void>> futures;
    for (const auto& file_path : files) {
        futures.push_back(std::async(std::launch::async, [&, file_path]() {
            fs::path output_file = output_dir / file_path.filename();
            if (!same_format) output_file.replace_extension(".webp");
            uintmax_t old_size = fs::file_size(file_path);
            
            if (compress_image(file_path, output_file, quality, same_format)) {
                uintmax_t new_size = fs::file_size(output_file);
                double ratio = (1.0 - (double)new_size / old_size) * 100.0;
                
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << std::left << std::setw(30) << file_path.filename().string().substr(0, 29)
                          << std::setw(15) << format_size(old_size)
                          << std::setw(15) << format_size(new_size)
                          << std::fixed << std::setprecision(1) << ratio << "%" << std::endl;
            }
        }));
    }

    for (auto& f : futures) f.get();
    
    std::cout << std::string(80, '-') << std::endl << std::endl;
}

void print_usage() {
    std::cout << "Usage: imgcomp [quality] [mode]" << std::endl;
    std::cout << std::endl;
    std::cout << "Compresses all JPEG/PNG images inside directories prefixed with 'input'." << std::endl;
    std::cout << std::endl;
    std::cout << "  quality  Compression quality 0-100 (default: 40)" << std::endl;
    std::cout << "  mode     Set to 'same' to keep original format; otherwise all" << std::endl;
    std::cout << "           images are transcoded to WebP (default: webp)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  imgcomp            compress all input*/ folders to WebP at quality 40" << std::endl;
    std::cout << "  imgcomp 75         compress to WebP at quality 75" << std::endl;
    std::cout << "  imgcomp 50 same    keep original formats, quality 50" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "-h" || arg1 == "--help") {
            print_usage();
            return 0;
        }
    }

    std::cout << "\033[1;36m" << "  _____                     _ " << std::endl;
    std::cout << " |_   _|                   | |" << std::endl;
    std::cout << "   | |  _ __ ___   __ _  __| |" << std::endl;
    std::cout << "   | | | '_ ` _ \\ / _` |/ _` |" << std::endl;
    std::cout << "  _| |_| | | | | | (_| | (_| |" << std::endl;
    std::cout << " |_____|_| |_| |_|\\__, |\\__,_|" << std::endl;
    std::cout << "         COMPRESS |___/       " << "\033[0m" << std::endl << std::endl;

    int quality = 40;
    bool same_format = false;

    if (argc > 1) { try { quality = std::stoi(argv[1]); } catch (...) {} }
    if (argc > 2 && std::string(argv[2]) == "same") { same_format = true; }

    bool found = false;
    for (const auto& entry : fs::directory_iterator(fs::current_path())) {
        if (entry.is_directory() && entry.path().filename().string().find("input") == 0) {
            process_directory(entry.path(), quality, same_format);
            found = true;
        }
    }
    if (!found) std::cout << "No 'input*' folders found." << std::endl;
    return 0;
}
