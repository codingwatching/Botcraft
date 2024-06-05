#if PROTOCOL_VERSION > 765 /* > 1.20.4 */
#pragma once
#include "protocolCraft/Types/Components/DataComponentType.hpp"
#include "protocolCraft/Types/Components/Subtypes/SuspiciousStewEntry.hpp"

#include <vector>

namespace ProtocolCraft
{
    namespace Components
    {
        class DataComponentTypeSuspiciousStewEffects : public DataComponentType
        {
            DECLARE_FIELDS_TYPES(std::vector<SuspiciousStewEntry>);
            DECLARE_FIELDS_NAMES(Effects);
            DECLARE_READ_WRITE_SERIALIZE;

            GETTER_SETTER(Effects);
        };
    }
}
#endif