#pragma once

#include "ReflectedProperty.h"
#include "Reflector/Clang/Utils.h"


namespace Lumina
{
    class FReflectedEnumProperty : public FReflectedProperty
    {
    public:
        
        const char* GetTypeName() override
        {
            return "Enum";
        }
        
        void AppendDefinition(eastl::string& Stream) const override
        {
            eastl::string CustomData = "Construct_CEnum_" + ClangUtils::MakeCodeFriendlyNamespace(TypeName);
            AppendPropertyDef(Stream, "Lumina::EPropertyFlags::None", "Lumina::EPropertyTypeFlags::Enum", CustomData);
        }

        const char* GetPropertyParamType() const override { return "FEnumPropertyParams"; }

        bool CanDeclareCrossModuleReferences() const override { return true; }
        void DeclareCrossModuleReference(const eastl::string& API, eastl::string& Stream) override
        {
            Stream += API;
            Stream += " Lumina::CEnum* Construct_CEnum_";
            Stream += ClangUtils::MakeCodeFriendlyNamespace(TypeName);
            Stream += "();\n";
        }

    };
    
}
