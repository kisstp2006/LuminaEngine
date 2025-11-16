#include "JoltPhysics.h"

#include "JoltPhysicsScene.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/Factory.h"


namespace Lumina::Physics
{
    static TUniquePtr<FJoltData> JoltData;
    
    static void JoltTraceCallback(const char* format, ...)
    {
        va_list list;
        va_start(list, format);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), format, list);

        if (JoltData)
        {
            JoltData->LastErrorMessage = buffer;
        }
        LOG_TRACE("Jolt Physics - {}", buffer);
    }
    
    void FJoltPhysicsContext::Initialize()
    {
        JPH::RegisterDefaultAllocator();

        JPH::Trace = JoltTraceCallback;

        JPH::Factory::sInstance = Memory::New<JPH::Factory>();

        JPH::RegisterTypes();
        
        JoltData = MakeUniquePtr<FJoltData>();

        JoltData->TemporariesAllocator = MakeUniquePtr<JPH::TempAllocatorImpl>(300 * 1024 * 1024);

        JoltData->JobThreadPool = MakeUniquePtr<JPH::JobSystemThreadPool>(2048, 8, 6);
        
    }

    void FJoltPhysicsContext::Shutdown()
    {

        JPH::UnregisterTypes();

        JoltData.reset();
        Memory::Delete(JPH::Factory::sInstance);
    }

    TUniquePtr<IPhysicsScene> FJoltPhysicsContext::CreatePhysicsScene(CWorld* World)
    {
        return MakeUniquePtr<FJoltPhysicsScene>(World);
    }

    JPH::TempAllocator* FJoltPhysicsContext::GetAllocator()
    {
        return JoltData->TemporariesAllocator.get();
    }

    JPH::JobSystemThreadPool* FJoltPhysicsContext::GetThreadPool()
    {
        return JoltData->JobThreadPool.get();
    }
}

namespace JPH
{
    ExternalProfileMeasurement::ExternalProfileMeasurement(const char* inName, uint32 inColor /* = 0 */)
        : mUserData{}
    {
        
    }
    ExternalProfileMeasurement::~ExternalProfileMeasurement()
    {
        
    }
}
