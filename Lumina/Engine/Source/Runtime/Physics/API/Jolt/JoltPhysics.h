#pragma once
#include "Physics/Physics.h"
#include <Jolt/Jolt.h>
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Core/TempAllocator.h"

namespace Lumina
{
    class FPhysicsScene;
}

namespace Lumina::Physics
{
    struct FJoltData
    {
        TUniquePtr<JPH::TempAllocator> TemporariesAllocator;
        TUniquePtr<JPH::JobSystemThreadPool> JobThreadPool;

        FString LastErrorMessage = "";
    };
    
    class FJoltPhysicsContext : public IPhysicsContext
    {
    public:

        void Initialize() override;
        void Shutdown() override;
        TUniquePtr<IPhysicsScene> CreatePhysicsScene(CWorld* World) override;

        static JPH::TempAllocator* GetAllocator();
        static JPH::JobSystemThreadPool* GetThreadPool();
        
    };
}
