#pragma once

#include "Archiver.h"
#include "Containers/Array.h"
#include "Core/Assertions/Assert.h"
#include "Memory/Memory.h"

namespace Lumina
{

    class FMemoryArchiver : public FArchive
    {
    public:

        void Seek(int64 InPos) final
        {
            Offset = InPos;
        }
        
        int64 Tell() final
        {
            return Offset;
        }
        
    protected:

        FMemoryArchiver()
            : FArchive()
            , Offset(0)
        {}
        
        int64 Offset;
    };
    
    class FMemoryReader : public FMemoryArchiver
    {
    public:

        explicit FMemoryReader(const TVector<uint8>& InBytes, bool bIsPersistent = false)
            : Bytes    (InBytes)
            , LimitSize(INT64_MAX)
        {
            this->SetFlag(EArchiverFlags::Reading);
        }

        int64 TotalSize() override
        {
            return std::min((int64)Bytes.size(), LimitSize);   
        }

        void SetLimitSize(int64 NewLimitSize)
        {
            LimitSize = NewLimitSize;
        }

        void Serialize(void* V, int64 Length) override
        {
            if ((Length) && !HasError())
            {
                if (Length <= 0)
                {
                    SetHasError(true);
                    return;
                }
                
                // Only serialize if we have the requested amount of data
                if (Offset + Length <= TotalSize())
                {
                    memcpy(V, &Bytes[(int32)Offset], Length);
                    Offset += Length;
                }
                else
                {
                    SetHasError(true);
                    LOG_ERROR("Archiver does not have enough size! Requested: {} - Total: {}", Offset + Length, TotalSize());
                }
            } 
        }

    private:

        const TVector<uint8>&   Bytes;
        int64                   LimitSize;
    
    };

    /**
     * Similar to FMemoryReader but owns the data.
     */
    class FBufferReader : public FArchive
    {
    public:
        FBufferReader(void* InData, int64 InSize, bool bFreeAfterClose)
            : ReaderData(InData)
            , ReaderPos(0)
            , ReaderSize(InSize)
            , bFreeOnClose(bFreeAfterClose)
        {
            this->SetFlag(EArchiverFlags::Reading);
        }

        ~FBufferReader() override
        {
            if (bFreeOnClose && ReaderData)
            {
                Memory::Free(ReaderData);
                ReaderData = nullptr;
            }
        }

        int64 Tell() final
        {
            return ReaderPos;
        }
        
        int64 TotalSize() final
        {
            return ReaderSize;
        }

        void Seek(int64 InPos) final
        {
            Assert(InPos >= 0)
            Assert(InPos <= ReaderSize)
            ReaderPos = InPos;
        }

        void Serialize(void* Data, int64 Length) override
        {
            Assert(ReaderPos >= 0)
            Assert(ReaderPos + Length <= ReaderSize)
            Memory::Memcpy(Data, (uint8*)ReaderData + ReaderPos, Length);
            ReaderPos += Length;
        }

    private:
        
        void*   ReaderData;
        int64   ReaderPos;
        int64   ReaderSize;
        bool    bFreeOnClose = false;
    };

        
    class FMemoryWriter : public FMemoryArchiver
    {
    public:

        FMemoryWriter(TVector<uint8>& InBytes, bool bSetOffset = false)
            :Bytes(InBytes)
        {
            this->SetFlag(EArchiverFlags::Writing);
        }

        int64 TotalSize() override { return Bytes.size(); }
        
        virtual void Serialize(void* Data, int64 Num) override
        {
            const int64 NumBytesToAdd = Offset + Num - Bytes.size();
            if (NumBytesToAdd > 0)
            {
                const int64 NewArrayCount = Bytes.size() + NumBytesToAdd;
                
                Bytes.resize(NewArrayCount, 0);
            }

            Assert((Offset + Num) <= (int64)Bytes.size())

            if (Num > 0)
            {
                Memory::Memcpy(&Bytes[Offset], Data, Num);
                Offset += Num;
            }
        }
    private:
        
        TVector<uint8>&	Bytes;
    };
}
