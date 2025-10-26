#pragma once

#include "Containers/Name.h"
#include "Containers/String.h"
#include "Core/Object/ObjectHandle.h"
#include "Core/Templates/CanBulkSerialize.h"
#include "Core/Templates/IsSigned.h"
#include "Core/Versioning/CoreVersion.h"
#include "glm/glm.hpp"
#include "Platform/Platform.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>

#include "Log/Log.h"
#include "Types/BitFlags.h"

namespace Lumina
{
    class FField;
}

enum class EArchiverFlags : uint8
{
    None        = 0,
    Reading     = 1,
    Writing     = 2,
    Compress    = 3,
    Encrypt     = 4,
    NoFields    = 5,
};

namespace Lumina
{
    class CObject;

    class FArchive
    {
    public:
    
        FArchive() = default;
        FArchive(const FArchive&) = default;
        FArchive& operator=(const FArchive& ArchiveToCopy) = default;
        virtual ~FArchive() = default;

    
        virtual void Seek(int64 InPos) { }
        virtual int64 Tell() { return 0; }
        virtual int64 TotalSize() { return 0; }

        virtual void Serialize(void* V, int64 Length) {}

        // Set, remove, and check flags
        FORCEINLINE void SetFlag(EArchiverFlags Flag) { Flags.SetFlag(Flag); }
        FORCEINLINE void RemoveFlag(EArchiverFlags Flag) { Flags.ClearFlag(Flag); }
        FORCEINLINE bool HasFlag(EArchiverFlags Flag) const { return Flags.IsFlagSet(Flag); }

        /** Is this archiver writing to a buffer */
        FORCEINLINE bool IsWriting() const { return HasFlag(EArchiverFlags::Writing); }

        /** Is this archiver reading from a buffer, not to be mistaken with reading from a class.
         * As this is used to write to a class, whilst reading from a buffer.
         * */
        FORCEINLINE bool IsReading() const { return HasFlag(EArchiverFlags::Reading); }

        FORCEINLINE void SetHasError(bool bIsError) { bHasError = bIsError; }
        FORCEINLINE bool HasError() const { return bHasError; }

        FORCEINLINE static FPackageFileVersion GetEngineVersion()
        {
            return GPackageFileLuminaVersion;
        }
    
        /** Returns the maximum size of data that this archive is allowed to serialize. */
        FORCEINLINE SIZE_T GetMaxSerializeSize() const { return ArMaxSerializeSize; }

        
        virtual FArchive& operator<<(CObject*& Value)
        {
            LOG_ERROR("Serializing CObjects is not supported by this archive.");
            return *this;
        }

        virtual FArchive& operator<<(FObjectHandle& Value)
        {
            LOG_ERROR("Serializing CObjects is not supported by this archive.");
            return *this;
        }

        virtual FArchive& operator<<(FField*& Value)
        {
            LOG_ERROR("Serializing FFields is not supported by this archive.");
            return *this;
        }
    
        virtual FArchive& operator<<(uint8& Value)
        {
            Serialize(&Value, 1);
            return *this;
        }

        virtual FArchive& operator<<(int8& Value)
        {
            Serialize(&Value, 1);
            return *this;
        }
    
        virtual FArchive& operator<<(uint16& Value)
        {
            ByteOrderSerialize(Value);
            return *this;
        }
    
        virtual FArchive& operator<<(int16& Value)
        {
            ByteOrderSerialize(reinterpret_cast<uint16&>(Value));
            return *this;
        }
    
        virtual FArchive& operator<<(uint32& Value)
        {
            ByteOrderSerialize(Value);
            return *this;
        }
    
        virtual FArchive& operator<<(int32& Value)
        {
            ByteOrderSerialize(reinterpret_cast<uint32&>(Value));
            return *this;
        }

        virtual FArchive& operator<<( bool& D)
        {
            SerializeBool(D);
            return *this;
        }

        virtual FArchive& operator<<(float& Value)
        {
            static_assert(sizeof(float) == sizeof(uint32), "Unexpected float size");
            uint32 Temp;
            std::memcpy(&Temp, &Value, sizeof(Temp));
            ByteOrderSerialize(Temp);
            std::memcpy(&Value, &Temp, sizeof(Value));
            return *this;
        }

        virtual FArchive& operator<<(double& Value)
        {
            static_assert(sizeof(double) == sizeof(uint64), "Unexpected double size");
            uint64 Temp;
            std::memcpy(&Temp, &Value, sizeof(Temp));
            ByteOrderSerialize(Temp);
            std::memcpy(&Value, &Temp, sizeof(Value));
            return *this;
        }


        virtual FArchive& operator<<(uint64& Value)
        {
            ByteOrderSerialize(Value);
            return *this;
        }

        virtual FArchive& operator<<(int64& Value)
        {
            ByteOrderSerialize(reinterpret_cast<uint64&>(Value));
            return *this;
        }

        virtual void SerializeBool(bool& D);
    
        virtual FArchive& operator<<(Lumina::FString& str)
        {
            if (IsReading())
            {
                SIZE_T SaveNum = 0;
                *this << SaveNum;
            
                if (SaveNum > GetMaxSerializeSize())
                {
                    SetHasError(true);
                    LOG_ERROR("Archiver is corrupted, string is too large! (Size: {0}, Max: {1})", SaveNum, GetMaxSerializeSize());
                    return *this;
                }
            
                if (SaveNum)
                {
                    str.clear();
                    str.resize(SaveNum);
                    Serialize(str.data(), (int64)SaveNum);
                }
                else
                {
                    str.clear();
                }
            }
            else
            {
                SIZE_T SaveNum = str.size();
                *this << SaveNum;

                if (SaveNum)
                {
                    Serialize(str.data(), (int64)SaveNum);
                }
            }
        
            return *this;
        }

        virtual FArchive& operator<<(FName& Name)
        {
            if (IsReading())
            {
                FString LoadedString;
                *this << LoadedString;
                Name = FName(LoadedString);
            }
            else
            {
                FString SavedString(Name.ToString());
                *this << SavedString;
            }

            return *this;
        }

        //-------------------------------------------------------------------------
        // Serialization for containers.
        //-------------------------------------------------------------------------

        template<typename ValueType>
        FArchive& operator << (TVector<ValueType>& Array)
        {
            SIZE_T SerializeNum = IsReading() ? 0 : Array.size();
            *this << SerializeNum;
        
            if (SerializeNum == 0)
            {
                // if we are loading, then we have to reset the size to 0, in case it isn't currently 0
                if (IsReading())
                {
                    Array.clear();
                }
            
                return *this;
            }
        
            if (SerializeNum > GetMaxSerializeSize())
            {
                SetHasError(true);
                LOG_ERROR("Archiver is corrupted, attempted to serialize {} array elements. Max is: {}", SerializeNum, GetMaxSerializeSize());
                return *this;
            }

            // If we don't need to perform per-item serialization, just read it in bulk
            if constexpr (sizeof(ValueType) == 1 || TCanBulkSerialize<ValueType>::Value)
            {
                if (IsReading())
                {
                    Array.resize(SerializeNum);
                }
            
                Serialize(Array.data(), SerializeNum * sizeof(ValueType));
            }
            else
            {
                if (IsReading())
                {
                    Array.clear();
                    Array.resize(SerializeNum);

                    for (SIZE_T i = 0; i < SerializeNum; i++)
                    {
                        *this << Array[i];
                    }
                }
                else
                {
                    for (SIZE_T i = 0; i < SerializeNum; i++)
                    {
                        *this << Array[i];
                    }
                }
            }

            return *this;
        }
        
        template<typename EnumType>
        FORCEINLINE FArchive& operator<<(EnumType& Value)
        requires (std::is_enum_v<EnumType>)
        {
            return *this << reinterpret_cast<std::underlying_type_t<EnumType>&>(Value);

        }

    private:
    
        template<typename T>
        FArchive& ByteOrderSerialize(T& Value)
        {
            static_assert(!TIsSigned<T>::Value, "To reduce the number of template instances, cast 'Value' to a uint16&, uint32& or uint64& prior to the call or use ByteOrderSerialize(void*, int32).");
        
            Serialize(&Value, sizeof(T));
            return *this;
        }



    private:

        TBitFlags<EArchiverFlags> Flags;
        uint8 bHasError:1;
        SIZE_T ArMaxSerializeSize = INT32_MAX;

    };

    inline void FArchive::SerializeBool(bool& D)
    {
        uint32 OldUBoolValue;

        OldUBoolValue = D ? 1 : 0;
        Serialize(&OldUBoolValue, sizeof(OldUBoolValue));

        if (OldUBoolValue > 1)
        {
            LOG_ERROR("Invalid boolean encountered while reading archive - stream is most likely corrupted.");

            SetHasError(true);
        }
        D = !!OldUBoolValue;
    }

    // GLM Vector Types
    inline FArchive& operator<<(FArchive& Ar, glm::vec2& v)
    {
        Ar << v.x << v.y;
        return Ar;
    }

    inline FArchive& operator<<(FArchive& Ar, glm::vec3& v)
    {
        Ar << v.x << v.y << v.z;
        return Ar;
    }

    inline FArchive& operator<<(FArchive& Ar, glm::vec4& v)
    {
        Ar << v.x << v.y << v.z << v.w;
        return Ar;
    }

    // GLM Integer Vector Types
    inline FArchive& operator<<(FArchive& Ar, glm::ivec2& v)
    {
        Ar << v.x << v.y;
        return Ar;
    }

    inline FArchive& operator<<(FArchive& Ar, glm::ivec3& v)
    {
        Ar << v.x << v.y << v.z;
        return Ar;
    }

    inline FArchive& operator<<(FArchive& Ar, glm::ivec4& v)
    {
        Ar << v.x << v.y << v.z << v.w;
        return Ar;
    }

    // GLM Unsigned Integer Vector Types
    inline FArchive& operator<<(FArchive& Ar, glm::uvec2& v)
    {
        Ar << v.x << v.y;
        return Ar;
    }

    inline FArchive& operator<<(FArchive& Ar, glm::uvec3& v)
    {
        Ar << v.x << v.y << v.z;
        return Ar;
    }

    inline FArchive& operator<<(FArchive& Ar, glm::uvec4& v)
    {
        Ar << v.x << v.y << v.z << v.w;
        return Ar;
    }

    // GLM 16-bit Unsigned Integer Vector Types
    inline FArchive& operator<<(FArchive& Ar, glm::u16vec2& v)
    {
        Ar << v.x << v.y;
        return Ar;
    }

    inline FArchive& operator<<(FArchive& Ar, glm::u16vec3& v)
    {
        Ar << v.x << v.y << v.z;
        return Ar;
    }

    inline FArchive& operator<<(FArchive& Ar, glm::u16vec4& v)
    {
        Ar << v.x << v.y << v.z << v.w;
        return Ar;
    }

    // GLM Matrix Types
    inline FArchive& operator<<(FArchive& Ar, glm::mat2& m)
    {
        Ar << m[0] << m[1];
        return Ar;
    }

    inline FArchive& operator<<(FArchive& Ar, glm::mat3& m)
    {
        Ar << m[0] << m[1] << m[2];
        return Ar;
    }

    inline FArchive& operator<<(FArchive& Ar, glm::mat4& m)
    {
        Ar << m[0] << m[1] << m[2] << m[3];
        return Ar;
    }

    // GLM Quaternion
    inline FArchive& operator<<(FArchive& Ar, glm::quat& q)
    {
        Ar << q.x << q.y << q.z << q.w;
        return Ar;
    }

    inline FArchive& operator << (FArchive& Ar, FBitFlags& Data)
    {
        Ar << Data.Flags;
        return Ar;
    }
    
    
}





// Example usage for a custom struct
#if 0
struct MyStruct
{
    int x;
    float y;
    FString name;

    friend FArchive& operator<<(FArchive& archive, MyStruct& s)
    {
        archive << s.x << s.y << s.name;
        return archive;
    }
};
#endif