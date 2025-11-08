#include "AssetRegistry.h"

#include "Core/Object/ObjectRedirector.h"
#include "Core/Object/Package/Package.h"
#include "Paths/Paths.h"
#include "Platform/Filesystem/FileHelper.h"
#include "TaskSystem/TaskSystem.h"

namespace Lumina
{
    void FAssetRegistry::ProjectLoaded()
    {
        RunInitialDiscovery();
    }

    void FAssetRegistry::Initialize()
    {
        
    }

    void FAssetRegistry::Deinitialize()
    {
        ClearAssets();
    }

    void FAssetRegistry::RunInitialDiscovery()
    {
        LUMINA_PROFILE_SCOPE();
        
        ClearAssets();
        
        TFixedVector<FInlineString, 256> PackagePaths;
        for (const auto& [ID, Path] : Paths::GetMountedPaths())
        {
            for (const auto& Directory : std::filesystem::recursive_directory_iterator(Path.c_str()))
            {
                if (!Directory.is_directory() && Directory.path().extension() == ".lasset")
                {
                    PackagePaths.push_back(Directory.path().generic_string().c_str());
                }
                else if (Directory.is_directory())
                {
                    FName VirtualPath = Paths::ConvertToVirtualPath(Directory.path().generic_string().c_str());
                    AssetsByPath.insert(VirtualPath);
                }
            }
        }
        
        uint32 NumPackages = (uint32)PackagePaths.size();
        Task::AsyncTask(NumPackages, NumPackages / 4, [this, PackagePaths = std::move(PackagePaths)] (uint32 Start, uint32 End, uint32)
        {
            for (uint32 i = Start; i < End; ++i)
            {
                const FInlineString& PathString = PackagePaths[i];
                ProcessPackagePath(PathString);
            }
        
            if (End == PackagePaths.size())
            {
                OnInitialDiscoveryCompleted();
            }
        });
    }

    void FAssetRegistry::OnInitialDiscoveryCompleted()
    {
        LOG_INFO("Asset Registry Finished Initial Discovery: Num [{}]", Assets.size());
    }

    void FAssetRegistry::AssetCreated(CObject* Asset)
    {
        FScopeLock Lock(AssetsMutex);

        CPackage* Package = Asset->GetPackage();
        FName PathName = Package->GetName();
        LUM_ASSERT(AssetPackageMap.find(PathName) == AssetPackageMap.end())
        

        FAssetData* AssetData    = Memory::New<FAssetData>();
        AssetData->AssetName     = Asset->GetName();
        AssetData->PackageName   = Package->GetName();
        AssetData->FullPath      = Asset->GetQualifiedName();
        AssetData->AssetClass    = Asset->GetClass()->GetQualifiedName();

        Assets.emplace(AssetData);
        AssetPackageMap.emplace(PathName, AssetData);
        
        FString ParentPath = Paths::Parent(Package->GetName().ToString());
        if (!ParentPath.empty() && ParentPath.back() == ':')
        {
            ParentPath.append("//");
        }

        AssetsByPath[ParentPath].push_back(AssetData);
        
        GetOnAssetRegistryUpdated().Broadcast();
    }

    void FAssetRegistry::AssetDeleted(CPackage* Package)
    {
        FScopeLock Lock(AssetsMutex);

        FName PathName = Package->GetName();
        LUM_ASSERT(AssetPackageMap.find(PathName) != AssetPackageMap.end())

        FAssetData* Data = AssetPackageMap.at(PathName);
        AssetPackageMap.erase(PathName);
        Assets.erase(Data);

        FString ParentPath = Paths::Parent(PathName.ToString());
        if (!ParentPath.empty() && ParentPath.back() == ':')
        {
            ParentPath.append("//");
        }

        TVector<FAssetData*>& Paths = AssetsByPath[ParentPath];

        for (SIZE_T i = 0; i < Paths.size(); ++i)
        {
            FAssetData* Path = Paths[i];
            if (Path == Data)
            {
                Paths.erase_unsorted(Paths.begin() + i);
                break;
            }
        }

        if (Data)
        {
            Memory::Delete(Data);
        }
        
        GetOnAssetRegistryUpdated().Broadcast();
    }

    void FAssetRegistry::AssetRenamed(CObject* Asset, const FString& OldPackagePath)
    {
        FScopeLock Lock(AssetsMutex);

        CPackage* Package = Asset->GetPackage();
        CPackage* OldPackage = FindObject<CPackage>(nullptr, Paths::ConvertToVirtualPath(OldPackagePath));
        if (OldPackage)
        {
            FName OldPackageName = OldPackage->GetName();
            FName NewPackagePathName = Package->GetName();
        
            LUM_ASSERT(AssetPackageMap.find(Package->GetName()) == AssetPackageMap.end())
            LUM_ASSERT(AssetPackageMap.find(OldPackage->GetName()) != AssetPackageMap.end())
        
            FString NewParentPath = Paths::Parent(Package->GetName().ToString());

            auto MakeParentPackagePath = [] (FString& ParentPath)
            {
                if (!ParentPath.empty() && ParentPath.back() == ':')
                {
                    ParentPath.append("//");
                }
            };

            MakeParentPackagePath(NewParentPath);

            FAssetData* OldData = AssetPackageMap[OldPackage->GetName()];
            OldData->AssetClass = CObjectRedirector::StaticClass()->GetQualifiedName();
            
            FAssetData* Data    = Memory::New<FAssetData>();
            Data->AssetName     = Asset->GetName();
            Data->PackageName   = Package->GetName();
            Data->FullPath      = Asset->GetQualifiedName();
            Data->AssetClass    = Asset->GetClass()->GetQualifiedName();

            AssetsByPath[NewParentPath].push_back(Data);
            AssetPackageMap[Package->GetName()] = Data;
            Assets.emplace(Data);

            // Update dependency tracking for rename
            if (DependencyMap.find(OldPackageName) != DependencyMap.end())
            {
                DependencyMap[NewPackagePathName] = std::move(DependencyMap[OldPackageName]);
                DependencyMap.erase(OldPackageName);
            }

            // Update all references to the old name to point to the new name
            if (ReferenceMap.find(OldPackageName) != ReferenceMap.end())
            {
                ReferenceMap[NewPackagePathName] = std::move(ReferenceMap[OldPackageName]);
                ReferenceMap.erase(OldPackageName);
            }

            // Update references in other assets that pointed to this asset
            for (auto& [PackageName, References] : ReferenceMap)
            {
                if (References.find(OldPackageName) != References.end())
                {
                    References.erase(OldPackageName);
                    References.insert(NewPackagePathName);
                }
            }

        }

        GetOnAssetRegistryUpdated().Broadcast();
    }

    void FAssetRegistry::AssetSaved(CObject* Asset)
    {
        FScopeLock Lock(AssetsMutex);

        CPackage* Package = Asset->GetPackage();

        FString FullPathName = Paths::ResolveVirtualPath(Package->GetName().ToString());
        Paths::AddPackageExtension(FullPathName);
    
        TVector<uint8> FileBinary;
        if (!FileHelper::LoadFileToArray(FileBinary, FullPathName) || FileBinary.empty())
        {
            return;
        }

        FMemoryReader Reader(FileBinary);
        FPackageHeader Header;
        Reader << Header;
        LUM_ASSERT(Header.Tag == PACKAGE_FILE_TAG)

        Reader.Seek(Header.ImportTableOffset);

        TVector<FObjectImport> Imports;
        Reader << Imports;

        THashSet<FName> OldReferences = ReferenceMap[Package->GetName()];
    
        auto& Set = ReferenceMap[Package->GetName()];
        Set.clear();
    
        for (const FObjectImport& Import : Imports)
        {
            Set.insert(Import.Package);
        }

        UpdateDependencyTracking(Package->GetName(), Set);

        GetOnAssetRegistryUpdated().Broadcast();
    }

    const TVector<FAssetData*>& FAssetRegistry::GetAssetsForPath(const FName& Path)
    {
        return AssetsByPath[Path];
    }

    void FAssetRegistry::GetAssetsByClass(const CClass* Class, TVector<FAssetData>& OutAssets)
    {
        for (FAssetData* AssetData : Assets)
        {
            CClass* AssetClass = FindObject<CClass>(nullptr, AssetData->AssetClass);
            if (AssetClass == nullptr)
            {
                continue;
            }
            
            if (AssetClass->IsChildOf(Class))
            {
                OutAssets.push_back(*AssetData);
            }
        }
    }

    const THashSet<FName>& FAssetRegistry::GetReferences(const FName& Asset)
    {
        static THashSet<FName> EmptySet;
        auto It = ReferenceMap.find(Asset);
        if (It != ReferenceMap.end())
        {
            return It->second;
        }
        return EmptySet;
    }

    const THashSet<FName>& FAssetRegistry::GetDependencies(const FName& Asset)
    {
        static THashSet<FName> EmptySet;
        auto It = DependencyMap.find(Asset);
        if (It != DependencyMap.end())
        {
            return It->second;
        }
        return EmptySet;
    }

    void FAssetRegistry::UpdateDependencyTracking(const FName& Asset, const THashSet<FName>& NewReferences)
    {
        // Remove this asset from old dependencies
        if (ReferenceMap.find(Asset) != ReferenceMap.end())
        {
            for (const FName& OldRef : ReferenceMap[Asset])
            {
                if (DependencyMap.find(OldRef) != DependencyMap.end())
                {
                    DependencyMap[OldRef].erase(Asset);
                }
            }
        }
    
        // Add this asset to new dependencies
        for (const FName& NewRef : NewReferences)
        {
            DependencyMap[NewRef].insert(Asset);
        }
    }

    void FAssetRegistry::ProcessPackagePath(FStringView Path)
    {
        FScopeLock Lock(AssetsMutex);

        TVector<uint8> FileBinary;
        if (!FileHelper::LoadFileToArray(FileBinary, Path) || FileBinary.empty())
        {
            return;
        }

        FString VirtualPackagePath = Paths::ConvertToVirtualPath(Path.data());
        FName PackagePathName = VirtualPackagePath;
        FName PackageFileName = Paths::FileName(VirtualPackagePath, true);
        
        FAssetData* AssetData = Memory::New<FAssetData>();

        FMemoryReader Reader(FileBinary);
        FPackageHeader Header;
        Reader << Header;
        LUM_ASSERT(Header.Tag == PACKAGE_FILE_TAG)
        
        Reader.Seek(Header.ExportTableOffset);
        
        TVector<FObjectExport> Exports;
        Reader << Exports;
        
        FString PackageAssetClass;
        for (const FObjectExport& Export : Exports)
        {
            if (Export.ObjectName == PackageFileName)
            {
                PackageAssetClass = Export.ClassName.ToString();
                break;
            }
        }

        FString FileName = Paths::FileName(VirtualPackagePath);
        AssetData->AssetClass = PackageAssetClass;
        AssetData->AssetName = FileName;
        AssetData->FullPath = VirtualPackagePath + "." + FileName;
        AssetData->PackageName = VirtualPackagePath;

        Reader.Seek(Header.ImportTableOffset);

        TVector<FObjectImport> Imports;
        Reader << Imports;

        THashSet<FName> References;
        for (const FObjectImport& Import : Imports)
        {
            References.insert(Import.Package);
        }
        ReferenceMap[VirtualPackagePath] = References;

        // Build dependency map (what assets depend on the imports)
        for (const FName& ImportedPackage : References)
        {
            DependencyMap[ImportedPackage].insert(VirtualPackagePath);
        }

        Assets.emplace(AssetData);

        AssetPackageMap.emplace(PackagePathName, AssetData);

        FString ParentPath = Paths::Parent(VirtualPackagePath);
        if (!ParentPath.empty() && ParentPath.back() == ':')
        {
            ParentPath.append("//");
        }
        AssetsByPath[ParentPath].push_back(AssetData);
    }

    void FAssetRegistry::ClearAssets()
    {
        FScopeLock Lock(AssetsMutex);

        for (FAssetData* Asset : Assets)
        {
            Memory::Delete(Asset);
        }
        
        Assets.clear();
        AssetPackageMap.clear();
        AssetsByPath.clear();
        ReferenceMap.clear();
        DependencyMap.clear();

        BroadcastRegistryUpdate();
    }

    void FAssetRegistry::BroadcastRegistryUpdate()
    {
        OnAssetRegistryUpdated.Broadcast();
    }
}
