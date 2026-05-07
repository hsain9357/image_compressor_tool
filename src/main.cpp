#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <jpeglib.h>
#include <stdio.h>
#include <iomanip>
#include <set>
#include <map>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace fs = std::filesystem;

// Formatting for file sizes
std::string format_size(uintmax_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << (double)bytes / (1024 * 1024) << " MB";
    return ss.str();
}

// Optimized JPEG compression
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

// Lossy PNG compression via simple color quantization
// This simulates the "Lossy PNG" behavior of websites
void quantize_rgba(unsigned char* img, int width, int height, int quality) {
    // A quality of 40 means we reduce precision of colors
    // This allows the PNG compression (deflate) to find much larger patterns
    int shift = 8 - (quality / 15 + 1); // Quality 40 -> shift 3 bits
    if (shift < 1) shift = 1;
    if (shift > 6) shift = 6;

    for (int i = 0; i < width * height * 4; i++) {
        // Quantize each channel to reduce unique colors
        img[i] = (img[i] >> shift) << shift;
    }
}

bool compress_image(const fs::path& input_path, const fs::path& output_path, int quality) {
    int width, height, channels;
    // Load as 4 channels (RGBA) for consistent processing
    unsigned char* img = stbi_load(input_path.c_str(), &width, &height, &channels, 4);
    if (!img) return false;

    std::string ext = output_path.extension().string();
    for (auto& c : ext) c = std::tolower(c);

    bool success = false;
    if (ext == ".jpg" || ext == ".jpeg") {
        unsigned char* img3 = stbi_load(input_path.c_str(), &width, &height, &channels, 3);
        success = compress_jpeg(output_path, quality, width, height, img3);
        stbi_image_free(img3);
    } else if (ext == ".png") {
        // Apply lossy quantization to the PNG
        quantize_rgba(img, width, height, quality);
        
        // Save with maximum ZLIB compression effort
        stbi_write_png_compression_level = 9;
        success = stbi_write_png(output_path.c_str(), width, height, 4, img, width * 4);
    }

    stbi_image_free(img);
    return success;
}

void process_directory(const fs::path& input_dir, int quality) {
    std::string folder_name = input_dir.filename().string();
    fs::path output_dir = input_dir.parent_path() / (folder_name + "_com");
    if (!fs::exists(output_dir)) fs::create_directories(output_dir);

    std::cout << "\033[1;33mProcessing: " << input_dir.filename() << " (" << quality << "% quality)\033[0m" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << std::left << std::setw(30) << "File" << std::setw(15) << "Original" << std::setw(15) << "Compressed" << "Reduction" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            for (auto& c : ext) c = std::tolower(c);
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
                fs::path output_file = output_dir / entry.path().filename();
                uintmax_t old_size = fs::file_size(entry.path());
                if (compress_image(entry.path(), output_file, quality)) {
                    uintmax_t new_size = fs::file_size(output_file);
                    double ratio = (1.0 - (double)new_size / old_size) * 100.0;
                    std::cout << std::left << std::setw(30) << entry.path().filename().string().substr(0, 29)
                              << std::setw(15) << format_size(old_size)
                              << std::setw(15) << format_size(new_size)
                              << std::fixed << std::setprecision(1) << ratio << "%" << std::endl;
                }
            }
        }
    }
    std::cout << std::string(80, '-') << std::endl << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "\033[1;36m" << "  _____                     _ " << std::endl;
    std::cout << " |_   _|                   | |" << std::endl;
    std::cout << "   | |  _ __ ___   __ _  __| |" << std::endl;
    std::cout << "   | | | '_ ` _ \\ / _` |/ _` |" << std::endl;
    std::cout << "  _| |_| | | | | | (_| | (_| |" << std::endl;
    std::cout << " |_____|_| |_| |_|\\__, |\\__,_|" << std::endl;
    std::cout << "         COMPRESS |___/       " << "\033[0m" << std::endl << std::endl;

    int quality = 40;
    if (argc > 1) { try { quality = std::stoi(argv[1]); } catch (...) {} }

    bool found = false;
    for (const auto& entry : fs::directory_iterator(fs::current_path())) {
        if (entry.is_directory() && entry.path().filename().string().find("input") == 0) {
            process_directory(entry.path(), quality);
            found = true;
        }
    }
    if (!found) std::cout << "No 'input*' folders found." << std::endl;
    return 0;
}
