
#include "AssetRequest.h"

#include "Core/Object/Cast.h"
#include "Core/Object/ObjectRedirector.h"
#include "Core/Object/Package/Package.h"
#include "Paths/Paths.h"
#include "Platform/Filesystem/FileHelper.h"

namespace Lumina
{
    void FAssetRequest::WaitForTask()
    {
        GTaskSystem->WaitForTask(Task);
    }

    bool FAssetRequest::Process()
    {
        FString FullPath = Paths::RemoveExtension(AssetPath);

        // The extension is just an easy way to get the string after the "." delimiter.
        FString Name = Paths::GetExtension(AssetPath);
    
        FullPath = Paths::ResolveVirtualPath(FullPath);

        CPackage* Package = CPackage::LoadPackage(FullPath.c_str());
        if (Package == nullptr)
        {
            LOG_INFO("Failed to load package at path: {}", FullPath);
            return false;
        }
        
        PendingObject = Package->LoadObject(Name);
        if (PendingObject != nullptr)
        {
            if (PendingObject->HasAnyFlag(OF_NeedsPostLoad))
            {
                PendingObject->PostLoad();
                PendingObject->ClearFlags(OF_NeedsPostLoad);
            }
        
            // Recursively follow redirectors until we hit a non-redirector object
            while (CObjectRedirector* Redirector = Cast<CObjectRedirector>(PendingObject))
            {
                CObject* RedirectedObject = Redirector->RedirectionObject;
            
                if (RedirectedObject == nullptr)
                {
                    break;
                }
            
                if (RedirectedObject->HasAnyFlag(OF_NeedsLoad))
                {
                    // Need to load the redirected object's package if necessary
                    CPackage* RedirectedPackage = RedirectedObject->GetPackage();
                    if (RedirectedPackage != nullptr)
                    {
                        RedirectedPackage->LoadObject(RedirectedObject);
                    }
                }
            
                if (RedirectedObject->HasAnyFlag(OF_NeedsPostLoad))
                {
                    RedirectedObject->PostLoad();
                    RedirectedObject->ClearFlags(OF_NeedsPostLoad);
                }
            
                PendingObject = RedirectedObject;
            }
        }
    
        return true;
    }
}
 