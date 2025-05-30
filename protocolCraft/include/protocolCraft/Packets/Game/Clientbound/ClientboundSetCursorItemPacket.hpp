#if PROTOCOL_VERSION > 767 /* > 1.21.1 */
#pragma once

#include "protocolCraft/BasePacket.hpp"
#include "protocolCraft/Types/Item/Slot.hpp"

namespace ProtocolCraft
{
    class ClientboundSetCursorItemPacket : public BasePacket<ClientboundSetCursorItemPacket>
    {
    public:
        static constexpr std::string_view packet_name = "Set Cursor Item";

        SERIALIZED_FIELD(Contents, Slot);

        DECLARE_READ_WRITE_SERIALIZE;
    };
}
#endif
