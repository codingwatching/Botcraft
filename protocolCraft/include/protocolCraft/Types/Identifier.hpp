#pragma once

#include "protocolCraft/NetworkType.hpp"

namespace ProtocolCraft
{
    class Identifier : public NetworkType
    {
        DECLARE_FIELDS(
            (std::string, std::string),
            (Namespace,   Name)
        );
        DECLARE_SERIALIZE;

        GETTER_SETTER(Namespace);
        GETTER_SETTER(Name);

    public:
        bool operator <(const Identifier& rhs) const
        {
            return GetName() < rhs.GetName() ||
                (GetName() == rhs.GetName() && GetNamespace() < rhs.GetNamespace());
        }

        std::string GetFull() const
        {
            if (GetNamespace().empty())
            {
                return "minecraft:" + GetName();
            }
            else
            {
                return GetNamespace() + ':' + GetName();
            }
        }

    protected:
        virtual void ReadImpl(ReadIterator& iter, size_t& length) override
        {
            const std::string str = ReadData<std::string>(iter, length);
            const size_t split = str.find(':');

            if (split == std::string::npos)
            {
                SetNamespace("");
                SetName(str);
            }
            else
            {
                SetNamespace(str.substr(0, split));
                SetName(str.substr(split + 1));
            }
        }

        virtual void WriteImpl(WriteContainer& container) const override
        {
            if (GetNamespace().empty())
            {
                WriteData<std::string>(GetName(), container);
            }
            else
            {
                WriteData<std::string>(GetNamespace() + ':' + GetName(), container);
            }
        }
    };
} // ProtocolCraft
