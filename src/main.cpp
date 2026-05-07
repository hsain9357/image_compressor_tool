#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <jpeglib.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace fs = std::filesystem;

bool compress_jpeg(const fs::path& input_path, const fs::path& output_path, int quality, int width, int height, unsigned char* img) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE* outfile;
    JSAMPROW row_pointer[1];
    int row_stride;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    if ((outfile = fopen(output_path.c_str(), "wb")) == NULL) {
        return false;
    }
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = width * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &img[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
    return true;
}

bool compress_image(const fs::path& input_path, const fs::path& output_path, int quality) {
    int width, height, channels;
    unsigned char* img = stbi_load(input_path.c_str(), &width, &height, &channels, 0);
    if (!img) {
        std::cerr << "Failed to load: " << input_path << std::endl;
        return false;
    }

    std::string ext = output_path.extension().string();
    for (auto& c : ext) c = std::tolower(c);

    bool success = false;
    if (ext == ".jpg" || ext == ".jpeg") {
        // If it's a JPEG, ensure we have 3 channels for jpeglib or reload if necessary
        if (channels != 3) {
            stbi_image_free(img);
            img = stbi_load(input_path.c_str(), &width, &height, &channels, 3);
        }
        success = compress_jpeg(input_path, output_path, quality, width, height, img);
    } else if (ext == ".png") {
        // PNG quality is 0-9 (compression level). We map 100-quality to 0-9.
        // 40 quality (user default) -> 60% compression effort -> roughly level 6.
        int z_level = (100 - quality) / 10; 
        if (z_level > 9) z_level = 9;
        if (z_level < 0) z_level = 0;
        stbi_write_png_compression_level = z_level;
        success = stbi_write_png(output_path.c_str(), width, height, channels, img, width * channels);
    } else {
        // Fallback for other formats (BMP, TGA) supported by stb_image_write
        success = stbi_write_jpg(output_path.c_str(), width, height, channels, img, quality);
    }

    stbi_image_free(img);
    return success;
}

void process_directory(const fs::path& input_dir, int quality) {
    std::string folder_name = input_dir.filename().string();
    fs::path output_dir = input_dir.parent_path() / (folder_name + "_com");

    if (!fs::exists(output_dir)) fs::create_directories(output_dir);

    std::cout << "Processing: " << input_dir.filename() << " -> " << output_dir.filename() << " (" << quality << "%)" << std::endl;

    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            for (auto& c : ext) c = std::tolower(c);

            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
                fs::path output_file = output_dir / entry.path().filename();
                if (compress_image(entry.path(), output_file, quality)) {
                    std::cout << "  Done: " << entry.path().filename() << std::endl;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    int quality = 40;
    if (argc > 1) quality = std::stoi(argv[1]);

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
