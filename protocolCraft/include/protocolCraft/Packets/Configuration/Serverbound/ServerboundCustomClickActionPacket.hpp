#if PROTOCOL_VERSION > 770 /* > 1.21.5 */
#pragma once

#include "protocolCraft/BasePacket.hpp"

#include "protocolCraft/Types/Identifier.hpp"
#include "protocolCraft/Types/NBT/NBT.hpp"

namespace ProtocolCraft
{
    class ServerboundCustomClickActionConfigurationPacket : public BasePacket<ServerboundCustomClickActionConfigurationPacket>
    {
    public:
        static constexpr std::string_view packet_name = "Custom Click Action (Configuration)";

        SERIALIZED_FIELD(Id_, Identifier);
        SERIALIZED_FIELD(Payload, NBT::UnnamedValue);

        DECLARE_READ_WRITE_SERIALIZE;
    };
} //ProtocolCraft
#endif
