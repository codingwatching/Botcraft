#if PROTOCOL_VERSION > 754 /* > 1.16.5 */
#pragma once

#include "protocolCraft/BasePacket.hpp"

namespace ProtocolCraft
{
    class ServerboundMovePlayerPacketStatusOnly : public BasePacket<ServerboundMovePlayerPacketStatusOnly>
    {
    public:
        static constexpr std::string_view packet_name = "Move Player Status Only";

#if PROTOCOL_VERSION < 768 /* < 1.21.2 */
        SERIALIZED_FIELD(OnGround, bool);
#else
        SERIALIZED_FIELD_WITHOUT_GETTER_SETTER(Flags, std::bitset<2>);

    public:
        bool GetOnGround() const { return Flags[0]; }
        bool GetHorizontalCollision() const { return Flags[1]; }
        THIS& SetOnGround(const bool b) { Flags.set(0, b); return *this; }
        THIS& SetHorizontalCollision(const bool b) { Flags.set(1, b); return *this; }
#endif

        DECLARE_READ_WRITE_SERIALIZE;
    };
} //ProtocolCraft
#endif
