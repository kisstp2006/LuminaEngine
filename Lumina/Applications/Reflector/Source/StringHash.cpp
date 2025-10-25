#include "StringHash.h"

#include "EASTL/fixed_hash_map.h"
#include "Reflector/Clang/Utils.h"


namespace Lumina
{
    class FNameHashMap : public eastl::fixed_hash_map<uint64_t, eastl::string, 64>
    {
        eastl::hash_node<value_type, false> const* const* GetBuckets() const { return mpBucketArray; }
    };

    FNameHashMap* gNameCache = nullptr;
    

    void FStringHash::Initialize()
    {
        gNameCache = new FNameHashMap();
    }

    void FStringHash::Shutdown()
    {
        delete gNameCache;
        gNameCache = nullptr;
    }

    FStringHash::FStringHash(const char* Char)
    {
        if (Char != nullptr && strlen(Char) > 0)
        {
            ID = ClangUtils::HashString(Char);

            auto Itr = gNameCache->find(ID);
            if (Itr == gNameCache->end())
            {
                (*gNameCache)[ID] = eastl::string(Char);
            }
        }
    }
    

    FStringHash::FStringHash(const eastl::string& Str)
        :FStringHash(Str.c_str())
    {
    }

    FStringHash::FStringHash(const eastl::string_view& Str)
        :FStringHash(Str.data())
    {
    }

    bool FStringHash::IsNone() const
    {
        auto Itr = gNameCache->find(ID);
        return Itr == gNameCache->end();
    }

    eastl::string FStringHash::ToString() const
    {
        return eastl::string(c_str());
    }

    const char* FStringHash::c_str() const
    {
        if (ID == 0)
        {
            return nullptr;
        }

        auto Itr = gNameCache->find(ID);
        if (Itr != gNameCache->end())
        {
            return Itr->second.c_str();
        }

        return nullptr;
    }
}
