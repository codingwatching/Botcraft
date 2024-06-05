#if PROTOCOL_VERSION > 765 /* > 1.20.4 */
#pragma once
#include "protocolCraft/Types/Components/DataComponentType.hpp"
#include "protocolCraft/Types/Components/DataComponentTypeFireworkExplosion.hpp"

namespace ProtocolCraft
{
    namespace Components
    {
        class DataComponentTypeFireworks : public DataComponentType
        {
            DECLARE_FIELDS_TYPES(VarInt,         std::vector<DataComponentTypeFireworkExplosion>);
            DECLARE_FIELDS_NAMES(FlightDuration, Explosions);
            DECLARE_READ_WRITE_SERIALIZE;

            GETTER_SETTER(FlightDuration);
            GETTER_SETTER(Explosions);
        };
    }
}
#endif