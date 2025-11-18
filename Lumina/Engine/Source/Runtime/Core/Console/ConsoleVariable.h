#pragma once
#include "Containers/Array.h"
#include "Containers/String.h"
#include "Core/Templates/LuminaTemplate.h"
#include "Core/Templates/Optional.h"
#include "Core/Variant/Variant.h"

namespace Lumina
{
    
    using CVarValueType = TVariant<int32, float, bool, FStringView>;

    template<typename T, typename Variant>
    struct IsVariantMember;
    
    template<typename T, typename... Types>
    struct IsVariantMember<T, TVariant<Types...>>  : eastl::disjunction<eastl::is_same<T, Types>...> {};
    
    template<typename T>
    concept ValidConsoleVarType = IsVariantMember<T, CVarValueType>::value;

    namespace ConsoleHelpers
    {
        template<typename T>
        requires(eastl::is_same_v<T, int32>)
        TOptional<T> ParseValue(FStringView Str)
        {
            char* End = nullptr;
            long long Value = std::strtoll(Str.data(), &End, 10);
            if (End != Str.data() && Value >= eastl::numeric_limits<T>::min() && Value <= eastl::numeric_limits<T>::max())
            {
                return static_cast<T>(Value);
            }
            return eastl::nullopt;
        }

        template<typename T>
        requires(eastl::is_floating_point_v<T>)
        TOptional<T> ParseValue(FStringView Str)
        {
            char* End = nullptr;
            T Value = static_cast<T>(std::strtod(Str.data(), &End));
            if (End != Str.data())
            {
                return Value;
            }
            return eastl::nullopt;
        }

        template<typename T>
        requires(eastl::is_same_v<T, bool>)
        TOptional<T> ParseValue(FStringView Str)
        {
            if (Str == "true" || Str == "1" || Str == "True" || Str == "TRUE")
            {
                return true;
            }
            if (Str == "false" || Str == "0" || Str == "False" || Str == "FALSE")
            {
                return false;
            }
            return eastl::nullopt;
        }

        template<typename T>
        requires(eastl::is_same_v<T, FStringView> || eastl::is_convertible_v<T, FStringView>)
        TOptional<T> ParseValue(FStringView Str)
        {
            return Str;
        }

        
        
    }

    struct LUMINA_API FConsoleVariable
    {
        FStringView Name;
        FStringView Hint;
        CVarValueType* ValuePtr;
        CVarValueType DefaultValue;
        void (*OnChange)(const CVarValueType&);

        constexpr FConsoleVariable(FStringView InName, FStringView InHint, CVarValueType* InPtr, const CVarValueType& InDefault, void (*InCallback)(const CVarValueType&) = nullptr)
            : Name(InName)
            , Hint(InHint)
            , ValuePtr(InPtr)
            , DefaultValue(InDefault)
            , OnChange(InCallback)
        {}
    };



    

    class LUMINA_API FConsoleRegistry
    {
    public:

        using FConsoleContainer = THashMap<FStringView, FConsoleVariable>;

        
        static FConsoleRegistry& Get();


        void Register(FConsoleVariable&& Var);

        FConsoleVariable* Find(FStringView Name);

        const FConsoleContainer& GetAll() const;

        bool SetValueFromString(FStringView TargetName, FStringView StrValue);
        TOptional<FString> GetValueAsString(FStringView VariableName);

        void SaveToConfig();
        void LoadFromConfig();
    
    private:

        FConsoleContainer ConsoleVariables;
    };


    
    template<ValidConsoleVarType T>
    class TAutoConsoleVariable
    {
    public:
        
        static void DefaultCallback(const CVarValueType&) {}

        
        constexpr TAutoConsoleVariable(FStringView Name, T DefaultValue, FStringView Hint, void(*InCallback)(const CVarValueType&) = DefaultCallback)
            : Storage(DefaultValue)
        {
            FConsoleVariable Var(Name, Hint, &Storage, Storage, InCallback);
            FConsoleRegistry::Get().Register(Move(Var));
        }


        T& GetValue() const
        {
            return eastl::get<T>(Storage);
        }

        T* GetValuePtr() const
        {
            return eastl::get_if<T>(&Storage);
        }
        
    private:

        CVarValueType Storage;
    };
}
