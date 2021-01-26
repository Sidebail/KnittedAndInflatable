/*************************************************************************
 *
 * KINEMATICOUP CONFIDENTIAL
 * __________________
 *
 *  Copyright (2017-2020) KinematicSoup Technologies Incorporated
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of KinematicSoup Technologies Incorporated and its
 * suppliers, if any.  The intellectual and technical concepts contained
 * herein are proprietary to KinematicSoup Technologies Incorporated
 * and its suppliers and may be covered by Canadian and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from KinematicSoup Technologies Incorporated.
 */
#pragma once

#include <vector>
#include <functional>
#include <Log.h>

// flags for segment types. Their are 2 segment types: runs and blocks. Blocks are non-runs. End signifies the end of
// the data.
#define END 0
#define RUN 4
#define BLOCK 8

// Each header consists of flags and a segment size. The first byte is the flags. 2 bits encode how many bytes are used
// to encode the size of the segment, and 2 bits determine the segment type (block, or run, or end of data). These
// masks are used to get the size bits and the segment type bits.
#define SIZE_MASK 3
#define TYPE_MASK 12

// Minimum length of a run in bytes
#define MIN_RUN 5

#define LOG_CHANNEL "sfRunLength"

namespace KS {
namespace SceneFusion2 {

    /**
     * Run-length encoder/decoder.
     */
    class sfRunLength
    {
    public:
        /**
         * Comparator for comparing two values of type T.
         *
         * @param   T lhs
         * @param   T rhs
         * @return  bool true if the values are the same.
         */
        template<typename T>
        using Comparator = std::function<bool(T, T)>;

        /**
         * Encoder for encoding a T value onto a byte array.
         *
         * @param   const T& value to encode
         * @param   std::vector<uint8_t>& output to encode to.
         */
        template<typename T>
        using Encoder = std::function<void(const T&, std::vector<uint8_t>&)>;

        /**
         * Decoder for decoding a T value from a byte array.
         *
         * @param   const uint8_t* data to decode from.
         * @param   int number of bytes available in the data.
         * @param   T& outValue to decode to.
         * @return  int number of bytes read from the data. Negative values indicate an error and will stop further
         *          decoding.
         */
        template<typename T>
        using Decoder = std::function<int(const uint8_t*, int, T& outValue)>;

        /**
         * Run-length encodes a templated array.
         *
         * @param   const T* data to encode.
         * @param   int size - number of elements in the data array.
         * @param   std::vector<uint8_T>& output to write encoded data to.
         * @param   Encoder<T> encoder for encoding T elements. If not provided, T will be encoded as raw bytes.
         * @param   Comparator<T> comparator for comparing T elements. If not provided, elements will be compared
         *          with ==.
         * @param   int tsize - size of a T element after encoding. Used for determining the min run-length to be worth
         *          encoding as a run. If 0 or less, sizeof(T) will be used.
         */
        template<typename T>
        static void Encode(
            const T* data,
            int size,
            std::vector<uint8_t>& output,
            Encoder<T> encoder = nullptr,
            Comparator<T> comparator = nullptr,
            int tsize = 0)
        {
            if (tsize <= 0)
            {
                tsize = sizeof(T);
            }
            int blockStart = 0;
            int runStart = -1;
            bool isRun = false;
            for (int index = 0; index <= size; index++)
            {
                if (isRun)
                {
                    if (index >= size || !Compare(data, runStart, index, comparator))
                    {
                        isRun = false;
                        uint32_t count = index - runStart;
                        uint8_t flags = RUN;
                        uint8_t lenSize = GetNumBytesToEncode(count);
                        flags |= lenSize - 1;
                        output.push_back(flags);
                        Write(count, lenSize, output);

                        if (encoder == nullptr)
                        {
                            int numBytes = sizeof(T);
                            int offset = output.size();
                            output.resize(output.size() + numBytes);
                            void* destPtr = (void*)(output.data() + offset);
                            void* srcPtr = (void*)(data + runStart);
                            std::memcpy(destPtr, srcPtr, numBytes);
                        }
                        else
                        {
                            encoder(data[runStart], output);
                        }

                        runStart = index;
                        blockStart = index;
                    }
                }
                else if (runStart < 0 || (index < size && !Compare(data, runStart, index, comparator)))
                {
                    runStart = index;
                }
                else if ((index - runStart + 1) * tsize >= MIN_RUN || index >= size)
                {
                    isRun = true;
                    uint32_t count = (index < size ? runStart : size) - blockStart;
                    if (count > 0)
                    {
                        uint8_t flags = BLOCK;
                        uint8_t lenSize = GetNumBytesToEncode(count);
                        flags |= lenSize - 1;
                        output.push_back(flags);
                        Write(count, lenSize, output);

                        if (encoder == nullptr)
                        {
                            int numBytes = count * sizeof(T);
                            int offset = output.size();
                            output.resize(output.size() + numBytes);
                            void* destPtr = (void*)(output.data() + offset);
                            void* srcPtr = (void*)(data + blockStart);
                            std::memcpy(destPtr, srcPtr, numBytes);
                        }
                        else
                        {
                            for (int i = 0; i < (int)count; i++)
                            {
                                encoder(data[blockStart + i], output);
                            }
                        }
                    }
                }
            }
            output.push_back(END);
        }

        /**
         * Decodes templated run-length-encoded data.
         *
         * @param   const uint8_t* data to decode.
         * @param   int size of the encoded data.
         * @param   std::vector<T>& output to decode to. T must be the same size as the type that was orginally
         *          encoded.
         * @param   Decoder<T> decoder for decoding a T element. If not provided, elements will be decoded from raw
         *          bytes.
         */
        template<typename T>
        static void Decode(const uint8_t* data, int size, std::vector<T>& output, Decoder<T> decoder = nullptr)
        {
            int index = 0;
            while (index < size)
            {
                uint8_t flags = data[index++];
                uint8_t type = flags & TYPE_MASK;
                switch (type)
                {
                    case RUN:
                    {
                        uint8_t lenSize = (flags & SIZE_MASK) + 1;
                        if (index + lenSize > size)
                        {
                            index = size;
                            break;
                        }
                        uint32_t count = Read(lenSize, data, index);

                        T value;
                        if (decoder == nullptr)
                        {
                            int numBytes = sizeof(T);
                            if (index + numBytes > size)
                            {
                                index = size;
                                break;
                            }
                            void* destPtr = (void*)&value;
                            void* srcPtr = (void*)(data + index);
                            std::memcpy(destPtr, srcPtr, numBytes);
                            index += numBytes;
                        }
                        else
                        {
                            int amountRead = decoder(data + index, size - index, value);
                            if (amountRead < 0)
                            {
                                KS::Log::Error("Run length decoder error: decoder function returned " +
                                    std::to_string(amountRead), LOG_CHANNEL);
                                return;
                            }
                            index += amountRead;
                        }
                        output.resize(output.size() + count, value);
                        break;
                    }
                    case BLOCK:
                    {
                        uint8_t lenSize = (flags & SIZE_MASK) + 1;
                        if (index + lenSize > size)
                        {
                            index = size;
                            break;
                        }
                        uint32_t count = Read(lenSize, data, index);

                        int offset = output.size();
                        output.resize(output.size() + count);
                        if (decoder == nullptr)
                        {
                            int numBytes = count * sizeof(T);
                            if (index + numBytes > size)
                            {
                                index = size;
                                break;
                            }
                            void* destPtr = (void*)(output.data() + offset);
                            void* srcPtr = (void*)(data + index);
                            std::memcpy(destPtr, srcPtr, numBytes);
                            index += numBytes;
                        }
                        else
                        {
                            for (int i = 0; i < (int)count; i++)
                            {
                                int amountRead = decoder(data + index, size - index, output[offset + i]);
                                if (amountRead < 0)
                                {
                                    KS::Log::Error("Run length decoder error: decoder function returned " +
                                        std::to_string(amountRead), LOG_CHANNEL);
                                    return;
                                }
                                index += amountRead;
                            }
                        }
                        break;
                    }
                    case END:
                    {
                        return;
                    }
                    default:
                    {
                        KS::Log::Error("Run length decoder error: invalid segment type", LOG_CHANNEL);
                        return;
                    }
                }
            }
            KS::Log::Error("Run length decoder error: tried to read passed the end of the data", LOG_CHANNEL);
        }

    private:
        /**
         * Compares two T values from an array to see if they are equivalent.
         *
         * @param   const T* data array to compare values from.
         * @param   int index1 of first value to compare
         * @param   int index2 of second value to compare
         * @param   Comparator<T> comparator for comparing the values. If nullptr, the default == will be used.
         * @return  bool true if the values are equal.
         */
        template<typename T>
        static bool Compare(const T* data, int index1, int index2, Comparator<T> comparator)
        {
            return comparator == nullptr ?
                data[index1] == data[index2] : comparator(data[index1], data[index2]);
        }

        /**
         * Gets the number of bytes needed to encode a value.
         *
         * @param   uint32_t value
         * @return  uint8_t number of bytes needed to encode the value.
         */
        static uint8_t GetNumBytesToEncode(uint32_t value)
        {
            uint8_t numBytes = 1;
            while (value > 255)
            {
                value >>= 8;
                numBytes++;
            }
            return numBytes;
        }

        /**
         * Writes a value to the output.
         *
         * @param   uint32_t value to write.
         * @param   uint8_t numBytes - number of bytes from the value to write. Should be between 1 and 4.
         * @param   std::vector<uint8_t>& output to write to.
         */
        static void Write(uint32_t value, uint8_t numBytes, std::vector<uint8_t>& output)
        {
            while (numBytes > 0)
            {
                uint8_t byteValue = (uint8_t)(value & 255);
                output.push_back(byteValue);
                value >>= 8;
                numBytes--;
            }
        }

        /**
         * Reads a value from the input.
         *
         * @param   uint8_t numBytes - number of bytes to read. should be between 1 and 4.
         * @param   const uint8_t* input to read from.
         * @param   int& index to read from. Will be incremented by the number of bytes read.
         * @return  uint32_t value read from the input.
         */
        static uint32_t Read(uint8_t numBytes, const uint8_t* input, int& index)
        {
            uint32_t value = 0;
            for (int i = 0; i < numBytes; i++)
            {
                uint32_t byteValue = (uint32_t)input[index++] << (8 * i);
                value |= byteValue;
            }
            return value;
        }
    };
} // SceneFusion2
} // KS

#undef END
#undef RUN
#undef BLOCK
#undef SIZE_MASK
#undef TYPE_MASK
#undef MIN_RUN
#undef LOG_CHANNEL