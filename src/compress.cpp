#include "compress.h"

#include <zlib.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstddef>

void zlib_compress( const std::string &input, std::vector<std::byte> &output )
{
    uLongf compressedSize = compressBound( input.size() );
    output.resize( compressedSize );

    int result = compress2(
                     reinterpret_cast<Bytef *>( output.data() ),
                     &compressedSize,
                     reinterpret_cast<const Bytef *>( input.data() ),
                     input.size(),
                     Z_BEST_SPEED
                 );

    if( result != Z_OK ) {
        throw std::runtime_error( "Zlib compression error" );
    }

    output.resize( compressedSize );
}

void zlib_decompress( const void *compressed_data, int compressed_size, std::string &output )
{
    // We need to guess at the decompressed size - we expect things to compress fairly well.
    uLongf decompressedSize = static_cast<uLongf>( compressed_size ) * 8;
    output.resize( decompressedSize );

    int result;
    do {
        result = uncompress(
                     reinterpret_cast<Bytef *>( output.data() ),
                     &decompressedSize,
                     reinterpret_cast<const Bytef *>( compressed_data ),
                     compressed_size
                 );

        if( result == Z_BUF_ERROR ) {
            decompressedSize *= 2; // Double the buffer size and retry
            output.resize( decompressedSize );
        } else if( result != Z_OK ) {
            throw std::runtime_error( "Zlib decompression failed" );
        }
    } while( result == Z_BUF_ERROR );

    output.resize( decompressedSize );
}