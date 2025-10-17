#include "Object.h"
#include "Class.h"

#include "ObjectRedirector.h"
#include "Core/Reflection/Type/LuminaTypes.h"
#include "Package/Package.h"

/** Low level CObject registration. */
extern Lumina::FClassRegistrationInfo Registration_Info_CClass_Lumina_CObject;
    
Lumina::CClass* Construct_CClass_Lumina_CObject()
{
    if (!Registration_Info_CClass_Lumina_CObject.OuterSingleton)
    {
        Registration_Info_CClass_Lumina_CObject.OuterSingleton = Lumina::CObject::StaticClass();
        Lumina::CObjectForceRegistration(Registration_Info_CClass_Lumina_CObject.OuterSingleton);
    }
    Assert(Registration_Info_CClass_Lumina_CObject.OuterSingleton->GetClass() != nullptr)
    return Registration_Info_CClass_Lumina_CObject.OuterSingleton;
}
    
IMPLEMENT_CLASS(Lumina, CObject)

namespace Lumina
{

    CObject::CObject()
    {
        FObjectInitializer* Initializer = FObjectInitializer::Get();
        Initializer->Object = this;
    };

    void CObject::Serialize(FArchive& Ar)
    {
        CClass* Class = GetClass();
        if (Class)
        {
            SerializeReflectedProperties(Ar);
        }
    }

    void CObject::SerializeReflectedProperties(FArchive& Ar)
    {
        GetClass()->SerializeTaggedProperties(Ar, this);
    }

    void CObject::Serialize(IStructuredArchive::FSlot Slot)
    {
        
    }

    void CObject::PostInitProperties()
    {
        
    }

    bool CObject::Rename(const FName& NewName, CPackage* NewPackage, bool bCreateRedirector)
    {
        FName SafeName = NewName;


        bool bShouldCreateRedirector = false;
        if (HasAnyFlag(OF_Public) && bCreateRedirector)
        {
            bool bPackage = GetClass() == CPackage::StaticClass();

            bShouldCreateRedirector = (bPackage == false);
        }

        FName OldName = GetName();
        CPackage* OldPackage = GetPackage();
        
        HandleNameChange(SafeName, NewPackage);

        if (bShouldCreateRedirector)
        {
            CObjectRedirector* Redirector = FindObject<CObjectRedirector>(OldPackage, OldName);

            if (Redirector == nullptr)
            {
                Redirector = NewObject<CObjectRedirector>(OldPackage, OldName, OF_Public);
            }

            Redirector->RedirectionObject = this;
        }
        
        return true;
    }
}
