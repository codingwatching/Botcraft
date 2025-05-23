#if PROTOCOL_VERSION > 766 /* > 1.20.6 */
#pragma once

#include "protocolCraft/BasePacket.hpp"
#include "protocolCraft/Types/ServerLinksUnstrustedEntry.hpp"

namespace ProtocolCraft
{
    class ClientboundServerLinksPacket : public BasePacket<ClientboundServerLinksPacket>
    {
    public:
        static constexpr std::string_view packet_name = "Server Links";

        SERIALIZED_FIELD(Links, std::vector<ServerLinksUnstrustedEntry>);

        DECLARE_READ_WRITE_SERIALIZE;
    };
} //ProtocolCraft
#endif
