#pragma once

#include "TaskScheduler.h"
#include "TaskTypes.h"
#include "Core/Singleton/Singleton.h"
#include "Core/Templates/LuminaTemplate.h"
#include "Core/Threading/Thread.h"
#include "Memory/Memory.h"
#include "Platform/GenericPlatform.h"

namespace Lumina
{
    LUMINA_API extern class FTaskSystem* GTaskSystem;

    namespace Task
    {
        struct FParallelRange
        {
            uint32 Start;
            uint32 End;
            uint32 Thread;
        };
    }

    class FTaskSystem
    {
    public:
        
        bool IsBusy() const { return Scheduler.GetIsShutdownRequested(); }
        uint32_t GetNumWorkers() const { return NumWorkers; }

        static void Initialize();
        static void Shutdown();
        
        void WaitForAll() 
        {
            Scheduler.WaitforAll(); 
        }

        /**
         * When scheduling tasks, the number specified is the number of iterations you want. EnkiTS will -
         * divide up the tasks between the available threads, it's important to note that these are *NOT*
         * executed in order. The ranges will be random, but they will all be executed only once, but if you -
         * need the index to be consistent. This will not work for you. Here's an example of a parallel for loop.
         *
         * Task::AsyncTask(10, [](uint32 Start, uint32 End, uint32 Thread)
         * {
         *      for(uint32 i = Start; i < End; ++i)
         *      {
         *          //.... [i] will be randomly distributed.
         *      }
         * });
         *
         * 
         * @param Num Number of executions.
         * @param Function Callback
         * @param Priority 
         * @return The task you can wait on, but should not be saved as it will be cleaned up automatically.
         */
        LUMINA_API FLambdaTask* ScheduleLambda(uint32 Num, uint32 MinRange, TaskSetFunction&& Function, ETaskPriority Priority = ETaskPriority::Medium)
        {
            if (Num == 0)
            {
                LOG_WARN("Task Size of [0] passed to task system.");
                return nullptr;
            }
            
            auto* Task = Memory::New<FLambdaTask>(Priority, Num, std::max(1u, MinRange), Move(Function));
            ScheduleTask(Task);
            return Task;
        }
        
        
        template<typename TFunc>
        void ParallelFor(uint32 Num, uint32 MinRange, TFunc&& Func, ETaskPriority Priority = ETaskPriority::Medium)
        {
            struct ParallelTask : ITaskSet
            {
                ParallelTask(TFunc&& InFunc, uint32 InNum, uint32 InMinRange)
                    : ITaskSet(InNum, InMinRange)
                    , Func(std::forward<TFunc>(InFunc))
                {
                }

                void ExecuteRange(TaskSetPartition range_, uint32_t ThreadNum) override
                {
                    if constexpr (eastl::is_invocable_v<TFunc, const Task::FParallelRange&>)
                    {
                        Func(Task::FParallelRange{range_.start, range_.end, ThreadNum});
                        return;
                    }
                    
                    for (uint32 i = range_.start; i < range_.end; ++i)
                    {
                        if constexpr (eastl::is_invocable_v<TFunc, uint32>)
                        {
                            Func(i);
                        }
                        else if constexpr (eastl::is_invocable_v<TFunc, uint32, uint32>)
                        {
                            Func(i, ThreadNum);
                        }
                    }
                }

                TFunc Func;
            };

            LUMINA_PROFILE_SECTION("Tasks::ParallelFor");
            ParallelTask Task = ParallelTask(std::forward<TFunc>(Func), Num, std::max(1u, MinRange));
            if (Num == 1)
            {
                Task.ExecuteRange(TaskSetPartition{0, 1}, Threading::GetThreadID());
                return;
            }
            
            Task.m_Priority = (enki::TaskPriority)Priority;
            ScheduleTask(&Task);
            WaitForTask(&Task);
        }
        
        LUMINA_API void ScheduleTask(ITaskSet* pTask)
        {
            LUMINA_PROFILE_SECTION("Tasks::ScheduleTask");
            Scheduler.AddTaskSetToPipe(pTask);
        }

        LUMINA_API void ScheduleTask(IPinnedTask* pTask)
        {
            LUMINA_PROFILE_SECTION("Tasks::ScheduleTask");
            Scheduler.AddPinnedTask(pTask);
        }

        LUMINA_API void WaitForTask(const ITaskSet* pTask, ETaskPriority Priority = ETaskPriority::Low)
        {
            LUMINA_PROFILE_SECTION("Tasks::WaitForTask");
            Scheduler.WaitforTask(pTask, (enki::TaskPriority)Priority);
        }

        LUMINA_API void WaitForTask(const IPinnedTask* pTask)
        {
            LUMINA_PROFILE_SECTION("Tasks::WaitForTask");
            Scheduler.WaitforTask(pTask);
        }
    
    private:
        
        enki::TaskScheduler     Scheduler;
        uint32                  NumWorkers = 0;
    };


    namespace Task
    {
        LUMINA_API FLambdaTask* AsyncTask(uint32 Num, uint32 MinRange, TaskSetFunction&& Function, ETaskPriority Priority = ETaskPriority::Medium);

        template<typename TIndex, typename TFunc>
        requires(eastl::is_integral_v<TIndex>)
        void ParallelFor(TIndex Num, uint32 MinRange, TFunc&& Func, ETaskPriority Priority = ETaskPriority::Medium)
        {
            GTaskSystem->ParallelFor(static_cast<uint32>(Num), MinRange, std::forward<TFunc>(Func), Priority);
        }
    }
}
