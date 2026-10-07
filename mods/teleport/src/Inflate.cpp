#include "Inflate.hpp"


namespace
{
    struct BitReader
    {
        const uint8_t* Data;
        size_t Size;
        size_t Position = 0;
        uint32_t Buffer = 0;
        int Count = 0;

        bool Bits(int n, uint32_t& value)
        {
            while (Count < n)
            {
                if (Position >= Size)
                {
                    return false;
                }
                Buffer |= static_cast<uint32_t>(Data[Position++]) << Count;
                Count += 8;
            }
            value = Buffer & ((1u << n) - 1);
            Buffer >>= n;
            Count -= n;
            return true;
        }
    };

    // Canonical Huffman table: symbol counts per code length, then symbols sorted by code.
    struct Huffman
    {
        uint16_t Counts[16] = {};
        uint16_t Symbols[320] = {};

        bool Build(const uint8_t* lengths, int count)
        {
            uint16_t offsets[16] = {};
            for (int i = 0; i < count; ++i)
            {
                ++Counts[lengths[i]];
            }
            Counts[0] = 0;
            for (int i = 1; i < 16; ++i)
            {
                offsets[i] = offsets[i - 1] + Counts[i - 1];
            }
            for (int i = 0; i < count; ++i)
            {
                if (lengths[i] != 0)
                {
                    Symbols[offsets[lengths[i]]++] = static_cast<uint16_t>(i);
                }
            }
            return true;
        }

        bool Decode(BitReader& reader, int& symbol) const
        {
            int code = 0, first = 0, index = 0;
            for (int length = 1; length < 16; ++length)
            {
                uint32_t bit = 0;
                if (!reader.Bits(1, bit))
                {
                    return false;
                }
                code |= static_cast<int>(bit);
                int count = Counts[length];
                if (code - count < first)
                {
                    symbol = Symbols[index + (code - first)];
                    return true;
                }
                index += count;
                first = (first + count) << 1;
                code <<= 1;
            }
            return false;
        }
    };

    constexpr uint16_t k_LengthBase[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
    constexpr uint8_t k_LengthExtra[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
    constexpr uint16_t k_DistanceBase[] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
    constexpr uint8_t k_DistanceExtra[] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

    bool InflateBlock(BitReader& reader, const Huffman& literals, const Huffman& distances, std::vector<uint8_t>& output)
    {
        while (true)
        {
            int symbol = 0;
            if (!literals.Decode(reader, symbol))
            {
                return false;
            }
            if (symbol < 256)
            {
                output.push_back(static_cast<uint8_t>(symbol));
                continue;
            }
            if (symbol == 256)
            {
                return true;
            }

            symbol -= 257;
            if (symbol >= 29)
            {
                return false;
            }
            uint32_t extra = 0;
            if (!reader.Bits(k_LengthExtra[symbol], extra))
            {
                return false;
            }
            size_t length = k_LengthBase[symbol] + extra;

            int distanceSymbol = 0;
            if (!distances.Decode(reader, distanceSymbol) || distanceSymbol >= 30 || !reader.Bits(k_DistanceExtra[distanceSymbol], extra))
            {
                return false;
            }
            size_t distance = k_DistanceBase[distanceSymbol] + extra;
            if (distance > output.size())
            {
                return false;
            }

            size_t from = output.size() - distance;
            for (size_t i = 0; i < length; ++i)
            {
                output.push_back(output[from + i]);
            }
        }
    }
}


bool ZlibDecompress(const uint8_t* data, size_t size, std::vector<uint8_t>& output)
{
    if (size < 2 || (data[0] & 0x0F) != 8)
    {
        return false; // not deflate
    }

    BitReader reader{ data + 2, size - 2 };
    uint32_t last = 0;
    do
    {
        uint32_t type = 0;
        if (!reader.Bits(1, last) || !reader.Bits(2, type))
        {
            return false;
        }

        if (type == 0)
        {
            // Stored block: byte aligned LEN / NLEN, then raw bytes.
            reader.Buffer = 0;
            reader.Count = 0;
            if (reader.Position + 4 > reader.Size)
            {
                return false;
            }
            uint16_t length = static_cast<uint16_t>(reader.Data[reader.Position] | (reader.Data[reader.Position + 1] << 8));
            reader.Position += 4;
            if (reader.Position + length > reader.Size)
            {
                return false;
            }
            output.insert(output.end(), reader.Data + reader.Position, reader.Data + reader.Position + length);
            reader.Position += length;
        }
        else if (type == 1)
        {
            uint8_t lengths[320] = {};
            for (int i = 0; i < 144; ++i) lengths[i] = 8;
            for (int i = 144; i < 256; ++i) lengths[i] = 9;
            for (int i = 256; i < 280; ++i) lengths[i] = 7;
            for (int i = 280; i < 288; ++i) lengths[i] = 8;
            Huffman literals, distances;
            literals.Build(lengths, 288);
            uint8_t distanceLengths[30] = {};
            for (uint8_t& length : distanceLengths) length = 5;
            distances.Build(distanceLengths, 30);
            if (!InflateBlock(reader, literals, distances, output))
            {
                return false;
            }
        }
        else if (type == 2)
        {
            uint32_t literalCount = 0, distanceCount = 0, codeCount = 0;
            if (!reader.Bits(5, literalCount) || !reader.Bits(5, distanceCount) || !reader.Bits(4, codeCount))
            {
                return false;
            }
            literalCount += 257;
            distanceCount += 1;
            codeCount += 4;

            static constexpr uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
            uint8_t codeLengths[19] = {};
            for (uint32_t i = 0; i < codeCount; ++i)
            {
                uint32_t value = 0;
                if (!reader.Bits(3, value))
                {
                    return false;
                }
                codeLengths[order[i]] = static_cast<uint8_t>(value);
            }
            Huffman codes;
            codes.Build(codeLengths, 19);

            uint8_t lengths[320] = {};
            uint32_t index = 0;
            while (index < literalCount + distanceCount)
            {
                int symbol = 0;
                if (!codes.Decode(reader, symbol))
                {
                    return false;
                }
                uint32_t repeat = 0;
                uint8_t value = 0;
                if (symbol < 16)
                {
                    lengths[index++] = static_cast<uint8_t>(symbol);
                    continue;
                }
                if (symbol == 16)
                {
                    if (index == 0 || !reader.Bits(2, repeat)) return false;
                    value = lengths[index - 1];
                    repeat += 3;
                }
                else if (symbol == 17)
                {
                    if (!reader.Bits(3, repeat)) return false;
                    repeat += 3;
                }
                else
                {
                    if (!reader.Bits(7, repeat)) return false;
                    repeat += 11;
                }
                if (index + repeat > literalCount + distanceCount)
                {
                    return false;
                }
                while (repeat-- > 0)
                {
                    lengths[index++] = value;
                }
            }

            Huffman literals, distances;
            literals.Build(lengths, static_cast<int>(literalCount));
            distances.Build(lengths + literalCount, static_cast<int>(distanceCount));
            if (!InflateBlock(reader, literals, distances, output))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    } while (last == 0);

    return true;
}
