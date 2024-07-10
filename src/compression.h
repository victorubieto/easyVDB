#pragma once

#include <iostream>

// Compression
#include "zlib.h"

void uncompressZlib(int64_t input_len, const uint8_t* input, int64_t output_len, uint8_t* output)
{
    static constexpr auto input_limit = static_cast<int64_t>(std::numeric_limits<uInt>::max());
    bool finished_;
    z_stream stream_;
    memset(&stream_, 0, sizeof(stream_));
    inflateInit(&stream_);
    stream_.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input));
    stream_.avail_in = static_cast<uInt>(std::min(input_len, input_limit));
    stream_.next_out = reinterpret_cast<Bytef*>(output);
    stream_.avail_out = static_cast<uInt>(std::min(output_len, input_limit));
    int ret;

    ret = inflate(&stream_, Z_SYNC_FLUSH);
    if (ret == Z_DATA_ERROR || ret == Z_STREAM_ERROR || ret == Z_MEM_ERROR) {
        std::cout << "zlib inflate failed: " << std::endl;
        return;
    }
    if (ret == Z_NEED_DICT) {
        std::cout << "zlib inflate failed (need preset dictionary): " << std::endl;
        return;
    }
    finished_ = (ret == Z_STREAM_END);
    if (ret == Z_BUF_ERROR) {
        // No progress was possible
        //DecompressResult{ 0, 0, true };
        return;
    }
    else {
        ret == Z_OK || ret == Z_STREAM_END;
        // Some progress has been made
        //DecompressResult{ input_len - stream_.avail_in, output_len - stream_.avail_out, false };
        return;
    }
    return;
}