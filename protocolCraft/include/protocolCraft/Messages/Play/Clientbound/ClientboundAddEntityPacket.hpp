#pragma once

#include "protocolCraft/BaseMessage.hpp"

namespace ProtocolCraft
{
    class ClientboundAddEntityPacket : public BaseMessage<ClientboundAddEntityPacket>
    {
    public:
#if   PROTOCOL_VERSION == 340 /* 1.12.2 */ || PROTOCOL_VERSION == 393 /* 1.13 */ ||  \
      PROTOCOL_VERSION == 401 /* 1.13.1 */ || PROTOCOL_VERSION == 404 /* 1.13.2 */ ||  \
      PROTOCOL_VERSION == 477 /* 1.14 */ || PROTOCOL_VERSION == 480 /* 1.14.1 */ ||  \
      PROTOCOL_VERSION == 485 /* 1.14.2 */ || PROTOCOL_VERSION == 490 /* 1.14.3 */ ||  \
      PROTOCOL_VERSION == 498 /* 1.14.4 */ || PROTOCOL_VERSION == 573 /* 1.15 */ ||  \
      PROTOCOL_VERSION == 575 /* 1.15.1 */ || PROTOCOL_VERSION == 578 /* 1.15.2 */ ||  \
      PROTOCOL_VERSION == 735 /* 1.16 */ || PROTOCOL_VERSION == 736 /* 1.16.1 */ ||  \
      PROTOCOL_VERSION == 751 /* 1.16.2 */ || PROTOCOL_VERSION == 753 /* 1.16.3 */ ||  \
      PROTOCOL_VERSION == 754 /* 1.16.4/5 */ || PROTOCOL_VERSION == 755 /* 1.17 */ ||  \
      PROTOCOL_VERSION == 756 /* 1.17.1 */ || PROTOCOL_VERSION == 757 /* 1.18/.1 */ ||  \
      PROTOCOL_VERSION == 758 /* 1.18.2 */ || PROTOCOL_VERSION == 759 /* 1.19 */ ||  \
      PROTOCOL_VERSION == 760 /* 1.19.1/2 */ || PROTOCOL_VERSION == 761 /* 1.19.3 */
        static constexpr int packet_id = 0x00;
#elif PROTOCOL_VERSION == 762 /* 1.19.4 */ || PROTOCOL_VERSION == 763 /* 1.20/.1 */ ||  \
      PROTOCOL_VERSION == 764 /* 1.20.2 */ || PROTOCOL_VERSION == 765 /* 1.20.3/4 */ ||  \
      PROTOCOL_VERSION == 766 /* 1.20.5/6 */ || PROTOCOL_VERSION == 767 /* 1.21 */
        static constexpr int packet_id = 0x01;
#else
#error "Protocol version not implemented"
#endif

        static constexpr std::string_view packet_name = "Add Entity";

#if PROTOCOL_VERSION < 477 /* < 1.14 */
        DECLARE_FIELDS_TYPES(VarInt, UUID, char, double, double, double, Angle, Angle, int,  short, short, short);
        DECLARE_FIELDS_NAMES(Id_,    Uuid, Type, X,      Y,      Z,      XRot,  YRot,  Data, Xa,    Ya,    Za);
#elif PROTOCOL_VERSION < 759 /* < 1.19 */
        DECLARE_FIELDS_TYPES(VarInt, UUID, VarInt, double, double, double, Angle, Angle, int,  short, short, short);
        DECLARE_FIELDS_NAMES(Id_,    Uuid, Type,   X,      Y,      Z,      XRot,  YRot,  Data, Xa,    Ya,    Za);
#else
        DECLARE_FIELDS_TYPES(VarInt, UUID, VarInt, double, double, double, Angle, Angle, Angle,    VarInt, short, short, short);
        DECLARE_FIELDS_NAMES(Id_,    Uuid, Type,   X,      Y,      Z,      XRot,  YRot,  YHeadRot, Data,   Xa,    Ya,    Za);
#endif
        DECLARE_READ_WRITE_SERIALIZE;

        GETTER_SETTER(Id_);
        GETTER_SETTER(Uuid);
        GETTER_SETTER(Type);
        GETTER_SETTER(X);
        GETTER_SETTER(Y);
        GETTER_SETTER(Z);
        GETTER_SETTER(XRot);
        GETTER_SETTER(YRot);
        GETTER_SETTER(Data);
        GETTER_SETTER(Xa);
        GETTER_SETTER(Ya);
        GETTER_SETTER(Za);
#if PROTOCOL_VERSION > 758 /* > 1.18.2 */
        GETTER_SETTER(YHeadRot);
#endif
    };
} //ProtocolCraft
