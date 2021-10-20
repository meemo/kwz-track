#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iterator>

#include "kwz-track.hpp"

#include <zlib.h>

std::vector<u8> decompressGZ(const char* data, std::size_t size) {
    // Adapted from decompress.hpp of the following:
    // https://github.com/mapbox/gzip-hpp

    std::vector<u8> output;

    z_stream inflate_s;

    inflate_s.zalloc = Z_NULL;
    inflate_s.zfree = Z_NULL;
    inflate_s.opaque = Z_NULL;
    inflate_s.avail_in = 0;
    inflate_s.next_in = Z_NULL;

    // The windowBits parameter is the base two logarithm of the window size (the size of the history buffer).
    // It should be in the range 8..15 for this version of the library.
    // Larger values of this parameter result in better compression at the expense of memory usage.
    // This range of values also changes the decoding type:
    //  -8  to -15        for raw deflate
    //   8  to  15        for zlib
    // ( 8  to  15 ) + 16 for gzip
    // ( 8  to  15 ) + 32 to automatically detect gzip/zlib header

    // auto with windowbits of 15
    constexpr int window_bits = 15 + 32;

    if (inflateInit2(&inflate_s, window_bits) != Z_OK) {
        throw std::runtime_error("inflate init failed");
    }

    inflate_s.next_in = reinterpret_cast<z_const Bytef*>(data);

    if (size > max_file_size || (size * 2) > max_file_size) {
        inflateEnd(&inflate_s);
        throw std::runtime_error("size may use more memory than intended when decompressing");
    }

    inflate_s.avail_in = static_cast<unsigned int>(size);
    std::size_t size_uncompressed = 0;
    do {
        std::size_t resize_to = size_uncompressed + 2 * size;
        if (resize_to > max_file_size) {
            inflateEnd(&inflate_s);
            throw std::runtime_error("size of output string will use more memory then intended when decompressing");
        }

        output.resize(resize_to);

        inflate_s.avail_out = static_cast<unsigned int>(2 * size);
        inflate_s.next_out = reinterpret_cast<Bytef*>(&output[0] + size_uncompressed);

        int ret = inflate(&inflate_s, Z_FINISH);

        if (ret != Z_STREAM_END && ret != Z_OK && ret != Z_BUF_ERROR) {
            std::string error_msg = inflate_s.msg;
            inflateEnd(&inflate_s);
            throw std::runtime_error(error_msg);
        }

        size_uncompressed += (2 * size - inflate_s.avail_out);

    } while (inflate_s.avail_out == 0);

    inflateEnd(&inflate_s);
    output.resize(size_uncompressed);

    return output;
}

void readFile(std::string path) {
    std::ifstream file(path, std::ios::binary);

    if (file) {
        // Stop eating newlines and white space in binary mode
        file.unsetf(std::ios::skipws);

        file.seekg(0, std::ios::end);
        std::streamoff file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        file_buffer.reserve(file_size);
        file_buffer.insert(file_buffer.begin(),
                           std::istream_iterator<uint8_t>(file),
                           std::istream_iterator<uint8_t>());

        file.close();

        // Check if file is gz compressed and decompress if needed.
        // gz compressed files start with 0x1F8B
        if (file_buffer[0] == 0x1F) {
            file_buffer = decompressGZ(reinterpret_cast<const char *>(file_buffer.data()), file_buffer.size());
        }

    }
    else {
        std::cout << "Failed to read file. " << std::endl;
        exit(-1);
    }
}

void getKSNMeta() {
    // Find sound section ("KSN" magic) offset
    // Traverse sections using sizes at the end of each header
    int offset = 0;

    while (offset < (int)file_buffer.size()) {
        if (file_buffer[offset + 1] == 'S' && file_buffer[offset + 2] == 'N') {
            ksn_offset = offset;
            break;
        }
        offset += getInt<u32>(offset + 4) + 8;
    }
    offset += getInt<u32>(offset - 4);

    // Get track sizes
    bgm_size = getInt<u32>(ksn_offset + 0xC);
    se_1_size = getInt<u32>(ksn_offset + 0x10);
    se_2_size = getInt<u32>(ksn_offset + 0x14);
    se_3_size = getInt<u32>(ksn_offset + 0x18);
    se_4_size = getInt<u32>(ksn_offset + 0x1C);

    // Calculate track offsets from sizes
    bgm_offset = ksn_offset + 0x24;
    se_1_offset = bgm_offset + bgm_size;
    se_2_offset = se_1_offset + se_1_size;
    se_3_offset = se_2_offset + se_2_size;
    se_4_offset = se_3_offset + se_3_size;
}

std::vector<s16> decodeTrack(int start_pos, int track_length, int initial_step_index) {
    std::vector<s16> output;

    s16 step_index = (s16)initial_step_index;
    s16 predictor = 0;
    s16 step = 0;
    s16 diff = 0;

    u8 sample = 0;
    u8 byte = 0;

    int bit_pos = 0;

    for (int buffer_pos = start_pos; buffer_pos <= (start_pos + track_length); buffer_pos++) {
        byte = file_buffer[buffer_pos];
        bit_pos = 0;

        while (bit_pos < 8) {
            if (step_index < 18 || bit_pos > 4) {
                // Decode 2 bit sample
                sample = byte & 0x3;

                step = ADPCM_STEP_TABLE[step_index];
                diff = step >> 3;

                if (sample & 1) diff += step;
                if (sample & 2) diff = -diff;

                predictor += diff;
                step_index += ADPCM_INDEX_TABLE_2[sample];

                byte >>= 2;
                bit_pos += 2;
            }
            else {
                // Decode 4 bit sample
                sample = byte & 0xF;

                step = ADPCM_STEP_TABLE[step_index];
                diff = step >> 3;

                if (sample & 1) diff += step >> 2;
                if (sample & 2) diff += step >> 1;
                if (sample & 4) diff += step;
                if (sample & 8) diff = -diff;

                predictor += diff;
                step_index += ADPCM_INDEX_TABLE_4[sample];

                byte >>= 4;
                bit_pos += 4;
            }

            // Clamp step index and predictor
            step_index = clampValue(step_index, 0, 79);
            predictor = clampValue(predictor, -2048, 2047);

            // Add to output buffer
            output.push_back(predictor * 16);
        }
    }

    return output;
}

void writeWAV(std::string path, std::vector<s16> audio) {
    std::ofstream output_file(path, std::ios::binary);

    // Generate and write WAV header
    wav_hdr wav;
    wav.chunk_size = (u32)(audio.size() + 36);
    wav.subchunk_2_size = (u32)(audio.size() * 2);
    output_file.write(reinterpret_cast<const char*>(&wav), sizeof(wav));

    // Write audio data
    output_file.write(reinterpret_cast<const char*>(&audio[0]), audio.size() * 2);

    output_file.close();
}

int main(int argc, char** argv) {
    std::string input_file_path = "";
    std::string output_file_path = "";

    // Read arguments
    if (argc == 4 || argc == 5) {
        track_index = std::atoi(argv[1]);
        input_file_path = std::string(argv[2]);
        output_file_path = std::string(argv[3]);
    }
    else {
        std::cout << "Invalid number of arguments passed!" << std::endl;
        exit(-1);
    }

    // Read file to buffer
    readFile(input_file_path);

    // Valid KWZ files begin with "KFH" (file header section magic)
    if (file_buffer[0] == 'K') {
        getKSNMeta();

        int step_index = 0;

        if (argc == 5) {
            step_index = clampValue(std::atoi(argv[4]), 0, 40);
        }

        switch (track_index) {
        case 0:
            writeWAV(output_file_path, decodeTrack(bgm_offset, bgm_size, step_index));
            break;
        case 1:
            writeWAV(output_file_path, decodeTrack(se_1_offset, se_1_size, step_index));
            break;
        case 2:
            writeWAV(output_file_path, decodeTrack(se_2_offset, se_2_size, step_index));
            break;
        case 3:
            writeWAV(output_file_path, decodeTrack(se_3_offset, se_3_size, step_index));
            break;
        case 4:
            writeWAV(output_file_path, decodeTrack(se_4_offset, se_4_size, step_index));
            break;
        }
    }
    else {
        std::cout << "Input file is invalid!" << std::endl;
    }
}
