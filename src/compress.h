#pragma once

#include <string>

#include "fstream_utils.h"

void zlib_compress( const std::string &input, std::vector<std::byte> &output );
void zlib_decompress( const void *compressed_data, int compressed_size, std::string &output );


