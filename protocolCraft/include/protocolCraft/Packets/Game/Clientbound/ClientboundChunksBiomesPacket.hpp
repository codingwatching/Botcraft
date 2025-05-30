#if PROTOCOL_VERSION > 761 /* > 1.19.3 */
#pragma once

#include "protocolCraft/BasePacket.hpp"

#include "protocolCraft/Types/ChunkBiomeData.hpp"

namespace ProtocolCraft
{
    class ClientboundChunksBiomesPacket : public BasePacket<ClientboundChunksBiomesPacket>
    {
    public:
        static constexpr std::string_view packet_name = "Chunks Biome";

        SERIALIZED_FIELD(ChunkBiomeData, std::vector<ProtocolCraft::ChunkBiomeData>);

        DECLARE_READ_WRITE_SERIALIZE;
    };
} //ProtocolCraft
#endif
