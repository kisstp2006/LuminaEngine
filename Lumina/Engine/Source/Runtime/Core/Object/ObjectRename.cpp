#include "ObjectRename.h"

#include <filesystem>

#include "Assets/AssetManager/AssetManager.h"
#include "Assets/AssetRegistry/AssetRegistry.h"
#include "Core/Engine/Engine.h"
#include "Package/Package.h"
#include "Paths/Paths.h"

namespace Lumina::ObjectRename
{
    EObjectRenameResult RenameObject(const FString& OldPath, FString NewPath)
    {
        try
        {
            bool bDirectory = Paths::IsDirectory(OldPath);
            
            if (!bDirectory)
            {
                NewPath += ".lasset";
            }
        
            if (std::filesystem::exists(NewPath.c_str()))
            {
                LOG_ERROR("Destination path already exists: {0}", NewPath);
                return EObjectRenameResult::Exists;
            }
            
            if (!bDirectory)
            {
                if (CPackage* OldPackage = CPackage::LoadPackage(OldPath.c_str()))
                {
                    // Make sure nothing is currently loading before executing this rename.
                    GEngine->GetEngineSubsystem<FAssetManager>()->FlushAsyncLoading();
                    
                    /** We need all objects to be loaded to rename a package */
                    LUM_ASSERT(OldPackage->LoadObjects())

                    bool bCreateRedirectors = true;
                    
                    FAssetRegistry* AssetRegistry = GEngine->GetEngineSubsystem<FAssetRegistry>();
                    const THashSet<FName>& Dependencies = AssetRegistry->GetDependencies(OldPackage->GetName());

                    if (Dependencies.empty())
                    {
                        bCreateRedirectors = false;
                    }
                    
                    FString OldAssetName = Paths::FileName(OldPath, true);
                    FString NewAssetName = Paths::FileName(NewPath, true);

                    CPackage* NewPackage = CPackage::CreatePackage(NewPath);
                    
                    TVector<CObject*> Objects;
                    GetObjectsWithPackage(OldPackage, Objects);

                    for (CObject* Object : Objects)
                    {
                        if (Object->IsAsset())
                        {
                            Object->Rename(NewAssetName, NewPackage, bCreateRedirectors);
                            AssetRegistry->AssetRenamed(Object, OldPath);
                        }
                        else
                        {
                            Object->Rename(Object->GetName(), NewPackage, false);
                        }
                    }

                    CPackage::SavePackage(NewPackage, NewPath);

                    if (bCreateRedirectors)
                    {
                        CPackage::SavePackage(OldPackage, OldPath);
                    }
                    else
                    {
                        CPackage::DestroyPackage(OldPath);
                    }
                }
            }
            else
            {
                std::filesystem::rename(OldPath.c_str(), NewPath.c_str());
            }
            
            return EObjectRenameResult::Success;
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            LOG_ERROR("Failed to rename file: {0}", e.what());
            return EObjectRenameResult::Failure;
        }
    }
}
