#include "TaskSystem.h"

#include "Core/Threading/Thread.h"
#include "Memory/Memory.h"

namespace Lumina
{
    LUMINA_API FTaskSystem* GTaskSystem = nullptr;

    namespace
    {
        void OnStartThread(uint32 threadNum)
        {
            FString ThreadName = "Background Worker: " + eastl::to_string(threadNum);
            Threading::SetThreadName(ThreadName.c_str());
        
            Memory::InitializeThreadHeap();
        }

        void OnStopThread(uint32_t threadNum)
        {
            Memory::ShutdownThreadHeap();
        }

        void ObWaitForTaskCompleteStart(uint32_t threadNum)
        {
            
        }

        void* CustomAllocFunc(size_t alignment, size_t size, void* userData_, const char* file_, int line_)
        {
            return Memory::Malloc(size, alignment);
        }

        void CustomFreeFunc(void* ptr, size_t size, void* userData_, const char* file_, int line_)
        {
            Memory::Free(ptr);
        }
    }
    
    void FTaskSystem::Initialize()
    {
        GTaskSystem = Memory::New<FTaskSystem>();

        GTaskSystem->NumWorkers                             = Threading::GetNumThreads() - 2;
        
        enki::TaskSchedulerConfig config;
        config.numTaskThreadsToCreate                       = Threading::GetNumThreads() - 2;
        config.customAllocator.alloc                        = CustomAllocFunc;
        config.customAllocator.free                         = CustomFreeFunc;
        config.profilerCallbacks.threadStart                = OnStartThread;
        config.profilerCallbacks.waitForTaskCompleteStart   = ObWaitForTaskCompleteStart;
        config.profilerCallbacks.threadStop                 = OnStopThread;

        GTaskSystem->Scheduler.Initialize(config);
    }

    void FTaskSystem::Shutdown()
    {
        GTaskSystem->Scheduler.WaitforAllAndShutdown();

        //while (!GTaskSystem->LambdaTaskPool.empty())
        //{
        //    FLambdaTask* Task = GTaskSystem->LambdaTaskPool.front();
        //    GTaskSystem->LambdaTaskPool.pop();
        //    Memory::Delete(Task);
        //}
        
        Memory::Delete(GTaskSystem);
        
        GTaskSystem = nullptr;
    }

    FLambdaTask* Task::AsyncTask(uint32 Num, uint32 MinRange, TaskSetFunction&& Function, ETaskPriority Priority)
    {
        return GTaskSystem->ScheduleLambda(Num, MinRange, std::move(Function), Priority);
    }
}
