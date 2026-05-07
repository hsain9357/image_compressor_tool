#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <jpeglib.h>
#include <webp/encode.h>
#include <libimagequant.h>
#include <stdio.h>
#include <iomanip>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace fs = std::filesystem;

std::string format_size(uintmax_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << (double)bytes / (1024 * 1024) << " MB";
    return ss.str();
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

bool compress_image(const fs::path& input_path, const fs::path& output_path, int quality) {
    int width, height, channels;
    // Load as RGBA to handle all formats consistently
    unsigned char* img = stbi_load(input_path.c_str(), &width, &height, &channels, 4);
    if (!img) return false;

    bool success = compress_webp(output_path, quality, width, height, img, 4);

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
                output_file.replace_extension(".webp");
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
