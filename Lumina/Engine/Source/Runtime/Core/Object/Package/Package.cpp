#include "Package.h"

#include "Assets/AssetRegistry/AssetRegistry.h"
#include "Core/Engine/Engine.h"
#include "Core/Object/Cast.h"
#include "Core/Object/Class.h"
#include "Core/Object/ObjectRedirector.h"
#include "Core/Object/GarbageCollection/GarbageCollector.h"
#include "Core/Profiler/Profile.h"
#include "Paths/Paths.h"
#include "Platform/Filesystem/FileHelper.h"

#include "Core/Serialization/Package/PackageSaver.h"
#include "Core/Serialization/Package/PackageLoader.h"
#include "TaskSystem/TaskSystem.h"
#include "Thumbnail/PackageThumbnail.h"


namespace Lumina
{
    IMPLEMENT_INTRINSIC_CLASS(CPackage, CObject, LUMINA_API)


    FObjectExport::FObjectExport(CObject* InObject)
    {
        ObjectName      = InObject->GetName();
        ClassName       = InObject->GetClass()->GetQualifiedName();
        Offset          = 0;
        Size            = 0;
        Object          = InObject;
    }

    FObjectImport::FObjectImport(CObject* InObject)
    {
        Package         = InObject->GetPackage()->GetName();
        ObjectName      = InObject->GetName();
        ClassName       = InObject->GetClass()->GetQualifiedName();
        Object          = InObject;
    }

    
    void CPackage::OnDestroy()
    {
        
    }

    CPackage* CPackage::CreatePackage(const FString& FileName)
    {
        FString VirtualPath = Paths::ConvertToVirtualPath(FileName.c_str());
        FName FileNameName = VirtualPath.c_str();
        
        CPackage* Package = FindObject<CPackage>(nullptr, FileNameName);
        if (Package)
        {
            LOG_WARN("Attempted to create a package that already existed {}", FileName);
            return Package;
        }
        
        Package = NewObject<CPackage>(nullptr, FileNameName);

        LOG_INFO("Created Package: \"{}\"", VirtualPath);
        
        Package->MarkDirty();
        
        return Package;
    }

    bool CPackage::DestroyPackage(const FString& PackageName)
    {
        // First we need to load the package before destroying it.*/
        CPackage* Package = LoadPackage(PackageName);
        if (Package == nullptr)
        {
            return false;
        }

        for (FObjectExport& Export : Package->ExportTable)
        {
            if (Export.Object)
            {
                Export.Object->MarkGarbage();
            }
        }
        
        Package->ExportTable.clear();

        GEngine->GetEngineSubsystem<FAssetRegistry>()->AssetDeleted(Package);
        
        Package->MarkGarbage();

        GarbageCollection::CollectGarbage();

        return true;
    }

    CPackage* CPackage::LoadPackage(const FName& FileName)
    {
        LUMINA_PROFILE_SCOPE();

        FString FullName = FileName.ToString();
        if (!Paths::HasExtension(FullName, "lasset"))
        {
            Paths::AddPackageExtension(FullName);
        }

        FString VirtualPath = Paths::ConvertToVirtualPath(FullName);
        CPackage* Package = FindObject<CPackage>(nullptr, VirtualPath.c_str());
        if (Package)
        {
            // Package is already loaded.
            return Package;
        }
        
        TVector<uint8> FileBinary;
        if (!FileHelper::LoadFileToArray(FileBinary, FullName))
        {
            return nullptr;
        }
        
        Package = NewObject<CPackage>(nullptr, VirtualPath.c_str());
        Package->CreateLoader(FileBinary);
        
        FPackageLoader& Reader = *(FPackageLoader*)Package->Loader.get();
        
        FPackageHeader PackageHeader;
        Reader << PackageHeader;

        if (PackageHeader.Tag != PACKAGE_FILE_TAG)
        {
            LOG_ERROR("Failed to load package, invalid data was given");
            Package->Loader.reset();
            return nullptr;
        }

        Reader.Seek((int64)PackageHeader.ImportTableOffset);
        Reader << Package->ImportTable;
        
        Reader.Seek((int64)PackageHeader.ExportTableOffset);
        Reader << Package->ExportTable;

        for (const FObjectImport& Name : Package->ImportTable)
        {
            
        }
        
        for (SIZE_T i = 0; i < Package->ExportTable.size(); ++i)
        {
            FObjectExport& Export = Package->ExportTable[i];
            
            CClass* ObjectClass = FindObject<CClass>(nullptr, Export.ClassName);
            
            CObject* Object = NewObject(ObjectClass, Package, Export.ObjectName);
            Object->SetFlag(OF_NeedsLoad);

            if (Object->IsAsset())
            {
                Object->SetFlag(OF_Public);
            }
            
            Object->LoaderIndex = FObjectPackageIndex::FromExport(static_cast<int64>(i)).GetRaw();

            Export.Object = Object;
        }

        LOG_INFO("Loaded Package: \"{}\" - ( [{}] Exports | [{}] Imports | [{}] Bytes)", Package->GetName(), Package->ExportTable.size(), Package->ImportTable.size(), Package->Loader->TotalSize());

        return Package;
    }

    bool CPackage::SavePackage(CPackage* Package, const FName& FileName)
    {
        LUMINA_PROFILE_SCOPE();

        FString PathString = FileName.ToString();
        if (!Paths::HasExtension(PathString, "lasset"))
        {
            Paths::AddPackageExtension(PathString);
        }
        
        Package->ExportTable.clear();
        Package->ImportTable.clear();
        
        TVector<uint8> FileBinary;
        FPackageSaver Writer(FileBinary, Package);

        
        FPackageHeader Header;
        Header.Tag = PACKAGE_FILE_TAG;
        Header.Version = 1;

        // Skip the header until we've built the tables.
        Writer.Seek(sizeof(FPackageHeader));
        
        // Build the save context (imports/exports)
        FSaveContext SaveContext(Package);
        Package->BuildSaveContext(SaveContext);

        for (CObject* Import : SaveContext.Imports)
        {
            Package->ImportTable.emplace_back(Import);
        }
        
        Header.ImportTableOffset = Writer.Tell();
        Writer << Package->ImportTable;
        
        
        for (CObject* Export : SaveContext.Exports)
        {
            Export->LoaderIndex = FObjectPackageIndex::FromExport((int64)Package->ExportTable.size()).GetRaw();
            Package->ExportTable.emplace_back(Export);
        }
        
        Header.ObjectDataOffset = Writer.Tell();

        for (size_t i = 0; i < Package->ExportTable.size(); ++i)
        {
            FObjectExport& Export = Package->ExportTable[i];
            LUM_ASSERT(Export.Object != nullptr)
            
            Export.Offset = Writer.Tell();
            
            Export.Object->Serialize(Writer);
            
            Export.Size = Writer.Tell() - Export.Offset;
            
        }
        
        Header.ExportTableOffset = Writer.Tell();
        Writer << Package->ExportTable;
        
        Header.ImportCount = (uint32)Package->ImportTable.size();
        Header.ExportCount = (uint32)Package->ExportTable.size();

        Header.ThumbnailDataOffset = Writer.Tell();
        if (Package->PackageThumbnail)
        {
            Package->PackageThumbnail->Serialize(Writer);
        }
        
        Writer.Seek(0);
        Writer << Header;

        // Reload the package loader to match the new file binary.
        Package->CreateLoader(FileBinary);

        if (!FileHelper::SaveArrayToFile(FileBinary, PathString))
        {
            return false;
        }
        
        LOG_INFO("Saved Package: \"{}\" - ( [{}] Exports | [{}] Imports | [{:.2f}] KiB)",
            Package->GetName(),
            Package->ExportTable.size(),
            Package->ImportTable.size(),
            static_cast<double>(FileBinary.size()) / 1024.0);
        
        return true;
    }

    void CPackage::CreateLoader(const TVector<uint8>& FileBinary)
    {
        void* HeapData = Memory::Malloc(FileBinary.size());
        Memory::Memcpy(HeapData, FileBinary.data(), FileBinary.size());
        Loader = MakeUniquePtr<FPackageLoader>(HeapData, FileBinary.size(), this);
    }

    FPackageLoader* CPackage::GetLoader() const
    {
        return (FPackageLoader*)Loader.get();
    }

    void CPackage::BuildSaveContext(FSaveContext& Context)
    {
        TVector<CObject*> ExportObjects;
        ExportObjects.reserve(20);
        GetObjectsWithPackage(this, ExportObjects);

        FSaveReferenceBuilderArchive Builder(&Context);
        for (CObject* Object : ExportObjects)
        {
            Builder << Object;
        }
    }

    void CPackage::CreateExports()
    {
        while (ExportIndex < ExportTable.size())
        {
            

            ++ExportIndex;
        }
    }

    void CPackage::CreateImports()
    {
        
    }

    void CPackage::LoadObject(CObject* Object)
    {
        LUMINA_PROFILE_SCOPE();
        if (!Object || !Object->HasAnyFlag(OF_NeedsLoad))
        {
            return;
        }

        CPackage* ObjectPackage = Object->GetPackage();
        
        // If this object's package comes from somewhere else, load it through there.
        if (ObjectPackage != this)
        {
            ObjectPackage->LoadObject(Object);
            return;
        }

        int64 LoaderIndex = FObjectPackageIndex(Object->LoaderIndex).GetArrayIndex();

        // Validate index
        if (LoaderIndex < 0 || LoaderIndex >= (int64)ExportTable.size())
        {
            LOG_ERROR("Invalid loader index {} for object {}", LoaderIndex, Object->GetQualifiedName());
            return;
        }

        FObjectExport& Export = ExportTable[LoaderIndex];

        if (!Loader)
        {
            LOG_ERROR("No loader set for package {}", GetName().ToString());
            return;
        }

        const int64 SavedPos = Loader->Tell();
        const int64 DataPos = Export.Offset;
        const int64 ExpectedSize = Export.Size;

        if (DataPos < 0 || ExpectedSize <= 0)
        {
            LOG_ERROR("Invalid export data for object {}. Offset: {}, Size: {}", Object->GetName().ToString(), DataPos, ExpectedSize);
            return;
        }
        
        // Seek to the data offset
        Loader->Seek(DataPos);
        
        Object->PreLoad();
        
        Object->Serialize(*Loader);
        
        const int64 ActualSize = Loader->Tell() - DataPos;
        
        if (ActualSize != ExpectedSize)
        {
            LOG_WARN("Mismatched size when loading object {}: expected {}, got {}", Object->GetName().ToString(), ExpectedSize, ActualSize);
        }
        
        Object->ClearFlags(OF_NeedsLoad);
        Object->SetFlag(OF_WasLoaded);

        Object->PostLoad();

        // Reset the state of the loader to the previous object.
        Loader->Seek(SavedPos);
    }

    bool CPackage::LoadObjects()
    {
        for (const FObjectExport& Export : ExportTable)
        {
            LoadObject(Export.Object);
        }

        return true;
    }

    CObject* CPackage::IndexToObject(const FObjectPackageIndex& Index)
    {
        if (Index.IsNull())
        {
            return nullptr;
        }
        
        if (Index.IsImport())
        {
            SIZE_T ArrayIndex = Index.GetArrayIndex();
            if (ArrayIndex >= ImportTable.size())
            {
                LOG_WARN("Failed to find an object in the import table {}", ArrayIndex);
                return nullptr;
            }
            FObjectImport& Import = ImportTable[ArrayIndex];
            FString FullPath = Paths::ResolveVirtualPath(Import.Package.ToString());
            CPackage* Package = LoadPackage(FName(FullPath));
            Import.Object = FindObject<CObject>(Package, Import.ObjectName);

            if (CObjectRedirector* Redirector = Cast<CObjectRedirector>(Import.Object))
            {
                LoadObject(Redirector);
                Import.Object = Redirector->RedirectionObject;
            }
            
            return ImportTable[ArrayIndex].Object;
        }

        if (Index.IsExport())
        {
            SIZE_T ArrayIndex = Index.GetArrayIndex();
            if (ArrayIndex >= ExportTable.size())
            {
                LOG_WARN("Failed to find an object in the export table {}", ArrayIndex);
                return nullptr;
            }
            
            return ExportTable[ArrayIndex].Object;
        }
        
        return nullptr;
    }

    FString CPackage::GetPackageFilename() const
    {
        return Paths::FileName(GetName().c_str(), true);
    }

    FString CPackage::GetFullPackageFilePath() const
    {
        FString Path = Paths::ConvertToVirtualPath(GetName().ToString());
        Paths::AddPackageExtension(Path);

        return Path;
    }

    FString CPackage::MakeUniquePackagePath(FStringView TestName)
    {
        FString UniquePath = Paths::MakeUniquePath(TestName);
        return MakeUniqueObjectName(CPackage::StaticClass(), nullptr, Paths::ConvertToVirtualPath(UniquePath)).ToString();
    }
}
