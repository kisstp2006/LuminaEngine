#pragma once
#include "Containers/Array.h"
#include "Containers/Name.h"
#include "Core/Assertions/Assert.h"
#include "Core/Profiler/Profile.h"
#include "Core/Singleton/Singleton.h"
#include "Core/Threading/Thread.h"


namespace Lumina
{
    class CClass;
    class CPackage;
    class CObjectBase;
}

namespace Lumina
{

    using FObjectHashBucket = TFixedHashSet<CObjectBase*, 4>;

    template<typename TKey>
    using TObjectHashMap = TFixedHashMap<TKey, FObjectHashBucket, 12>;

    class FObjectHashTables : public TSingleton<FObjectHashTables>
    {
    public:

        void AddObject(CObjectBase* Object);

        void RemoveObject(CObjectBase* Object);

        CObjectBase* FindObject(CClass* ObjectClass, CPackage* Package, const FName& ObjectName, bool bExactClass = false);

        void Clear();

        mutable FMutex Mutex;
        TObjectHashMap<FName>  ObjectNameHash;
        TObjectHashMap<CPackage*>  ObjectPackageHash;
        TObjectHashMap<CClass*>  ObjectClassHash;
    };
}
