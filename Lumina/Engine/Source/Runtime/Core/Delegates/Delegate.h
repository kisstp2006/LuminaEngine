#pragma once

#include "Containers/Array.h"
#include "Containers/Function.h"
#include "Core/Assertions/Assert.h"

namespace Lumina
{

    template<typename R, typename ... TArgs>
    class TBaseDelegate
    {
    public:

        using FFunc = TFunction<R(TArgs...)>;

        TBaseDelegate() = default;
        
        template<typename TFunc>
        requires(eastl::is_invocable_r_v<R, TFunc, TArgs...>)
        static TBaseDelegate CreateStatic(TFunc&& func)
        {
            TBaseDelegate Delegate;
            Delegate.Func = eastl::forward<TFunc>(func);
            return Delegate;
        }

        template<typename TObject, typename TMemFunc>
        static TBaseDelegate CreateMember(TObject* object, TMemFunc method)
        {
            TBaseDelegate Delegate;
            Delegate.Func = [object, method](TArgs... args) -> R
            {
                return (object->*method)(eastl::forward<TArgs>(args)...);
            };
            return Delegate;
        }

        template<typename TLambda>
        requires(eastl::is_invocable_r_v<R, TLambda, TArgs...>)
        static TBaseDelegate CreateLambda(TLambda&& lambda)
        {
            TBaseDelegate Delegate;
            Delegate.Func = eastl::forward<TLambda>(lambda);
            return Delegate;
        }

        bool IsBound() const { return static_cast<bool>(Func); }

        template<typename... TCallArgs>
        R Execute(TCallArgs&&... Args) const
        {
            LUM_ASSERT(Func)

            if constexpr (eastl::is_void_v<R>)
            {
                Func(eastl::forward<TCallArgs>(Args)...);
                return;
            }
            else
            {
                return Func(eastl::forward<TCallArgs>(Args)...);
            }
        }

        void Unbind() { Func = nullptr; }

        
    private:

        FFunc Func;
    };
    
    template<typename R, typename... Args>
    class TMulticastDelegate
    {
    public:
        using FBase = TBaseDelegate<R, Args...>;

        template<typename TFunc>
        void AddStatic(TFunc&& func)
        {
            InvocationList.push_back(FBase::CreateStatic(eastl::forward<TFunc>(func)));
        }

        template<typename TObject, typename TMemFunc>
        void AddMember(TObject* object, TMemFunc method)
        {
            InvocationList.push_back(FBase::CreateMember(object, method));
        }

        template<typename TLambda>
        void AddLambda(TLambda&& lambda)
        {
            InvocationList.push_back(FBase::CreateLambda(eastl::forward<TLambda>(lambda)));
        }
        
        template<typename... CallArgs>
        void Broadcast(CallArgs&&... args)
        {
            if constexpr (eastl::is_void_v<R>)
            {
                for (auto& d : InvocationList)
                {
                    if (d.IsBound())
                    {
                        d.Execute(eastl::forward<CallArgs>(args)...);
                    }
                }
            }
            else
            {
                for (auto& d : InvocationList)
                {
                    if (d.IsBound())
                    {
                        d.Execute(eastl::forward<CallArgs>(args)...);
                    }
                }
            }
        }

        template<typename... CallArgs>
        void BroadcastAndClear(CallArgs&&... args)
        {
            Broadcast(eastl::forward<CallArgs>(args)...);
            Clear();
        }

        void Clear()
        {
            InvocationList.clear();
            InvocationList.shrink_to_fit();
        }

    private:
        
        TVector<FBase> InvocationList;
    };
}

#define DECLARE_MULTICAST_DELEGATE(DelegateName, ...) \
struct DelegateName : public Lumina::TMulticastDelegate<void, __VA_ARGS__> {}

#define DECLARE_MULTICAST_DELEGATE_R(DelegateName, ...) \
struct DelegateName : public Lumina::TMulticastDelegate<__VA_ARGS__> {}