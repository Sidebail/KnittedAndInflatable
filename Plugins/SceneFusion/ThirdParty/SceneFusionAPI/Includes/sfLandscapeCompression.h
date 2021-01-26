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

#include <Exports.h>

namespace KS {
namespace SceneFusion2 {
    /**
     * Landscape data compression.
     */
    class sfLandscapeCompression
    {
    public:
        /**
         * Compresses the given height map data.
         *
         * @param   uint16_t* heightmapData
         * @param   int numQuads - total number of quads of the component to compress
         * @return  std::vector<uint8_t>
         */
        static std::vector<uint8_t> CompressHeightmap(uint16_t* heightmapData, int numQuads);

        /**
         * Compresses the given offset map data.
         *
         * @param   float* offsetmapData
         * @param   int numQuads - total number of quads of the component to compress
         * @return  std::vector<uint8_t>
         */
        static std::vector<uint8_t> CompressOffsetmap(float* offsetmapData, int numQuads);

        /**
         * Compresses the given weight map data.
         *
         * @param   uint8_t* weightmapData
         * @param   int numQuads - total number of quads of the component to compress
         * @param   int numLayers
         * @return  std::vector<uint8_t>
         */
        static std::vector<uint8_t> CompressWeightmap(uint8_t* weightmapData, int numQuads, int numLayers);

        /**
         * Decompresses height map data.
         *
         * @param   std::vector<uint8_t> encodedData
         * @param   int numQuads - total number of quads of the compressed component
         * @return  std::vector<uint16_t>
         */
        static std::vector<uint16_t> DecompressHeightmap(std::vector<uint8_t> encodedData, int numQuads);

        /**
         * Decompresses offset map data.
         *
         * @param   std::vector<uint8_t> encodedData
         * @param   int numQuads - total number of quads of the compressed component
         * @return  std::vector<float>
         */
        static std::vector<float> DecompressOffsetmap(std::vector<uint8_t> encodedData, int numQuads);

        /**
         * Decompresses weight map data.
         *
         * @param   std::vector<uint8_t> encodedData
         * @param   int numQuads - total number of quads of the compressed component
         * @param   int& numLayers
         * @return  std::vector<uint8_t>
         */
        static std::vector<uint8_t> DecompressWeightmap(
            std::vector<uint8_t> encodedData,
            int numQuads,
            int& numLayers);
    };
} // SceneFusion2
} // KS
#undef LOG_CHANNEL