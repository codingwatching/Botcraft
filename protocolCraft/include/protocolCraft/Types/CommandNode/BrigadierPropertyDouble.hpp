#if PROTOCOL_VERSION > 344 /* > 1.12.2 */
#pragma once

#include "protocolCraft/Types/CommandNode/BrigadierProperty.hpp"

namespace ProtocolCraft
{
    class BrigadierPropertyDouble : public BrigadierProperty
    {
        DEFINE_CONDITION(Flags1, GetFlags() & 0x01);
        DEFINE_CONDITION(Flags2, GetFlags() & 0x02);

        SERIALIZED_FIELD(Flags, char);
        SERIALIZED_FIELD(Min, Internal::Conditioned<double, &BrigadierPropertyDouble::Flags1>);
        SERIALIZED_FIELD(Max, Internal::Conditioned<double, &BrigadierPropertyDouble::Flags2>);

        DECLARE_READ_WRITE_SERIALIZE;
    };
}
#endif
