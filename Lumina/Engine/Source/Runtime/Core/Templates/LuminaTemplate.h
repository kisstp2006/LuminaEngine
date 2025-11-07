#pragma once
#include <type_traits>
#include "Core/Utils/NonCopyable.h"
#include "Platform/Platform.h"


namespace Lumina
{

    template <typename RefType, typename AssignedType = RefType>
	struct TGuardValue : private INonCopyable
	{
		NODISCARD TGuardValue(RefType& ReferenceValue, const AssignedType& NewValue)
		: RefValue(ReferenceValue), OriginalValue(ReferenceValue)
		{
			RefValue = NewValue;
		}
    	
		~TGuardValue()
		{
			RefValue = OriginalValue;
		}
    	
		/**
		 * Provides read-only access to the original value of the data being tracked by this struct
		 *
		 * @return	a const reference to the original data value
		 */
    	
		FORCEINLINE const AssignedType& GetOriginalValue() const
		{
			return OriginalValue;
		}
	
	private:
		RefType& RefValue;
		AssignedType OriginalValue;
	};
    

    template <typename T>
    FORCEINLINE T ImplicitConv(std::type_identity_t<T> Obj)
    {
        return Obj;
    }

    template <typename T>
    requires(eastl::is_lvalue_reference_v<T> && !eastl::is_const_v<T>)
    FORCEINLINE constexpr eastl::remove_reference<T>::type&& Move(T&& x) noexcept
    {
        return static_cast<eastl::remove_reference<T>::type&&>(eastl::forward<T>(x));
    }

    
    template <typename T>
    FORCEINLINE constexpr T&& Forward(eastl::remove_reference_t<T>& x) noexcept
    {
        return static_cast<T&&>(x);
    }


    template <typename T>
    requires(!eastl::is_lvalue_reference_v<T>)
    FORCEINLINE constexpr T&& Forward(eastl::remove_reference_t<T>&& x) noexcept
    {
        return static_cast<T&&>(x);
    }
}