#pragma once


#include <cstddef>
#include <cstdint>
#include <vector>


// Minimal zlib (RFC 1950/1951) decoder, enough to read the game's compressed bundle resources.
// Returns false on malformed input.
bool ZlibDecompress(const uint8_t* data, size_t size, std::vector<uint8_t>& output);
