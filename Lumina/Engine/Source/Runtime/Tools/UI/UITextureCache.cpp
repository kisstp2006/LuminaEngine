#include "UITextureCache.h"
#include "Core/Engine/Engine.h"
#include "EASTL/sort.h"
#include "ImGui/ImGuiRenderer.h"
#include "Paths/Paths.h"
#include "Renderer/RenderManager.h"
#include "Renderer/RHIGlobals.h"
#include "TaskSystem/TaskSystem.h"
#include "Tools/Import/ImportHelpers.h"

namespace Lumina
{
    FUITextureCache::FUITextureCache()
    {
        FName SquareTexturePath = Paths::GetEngineResourceDirectory() + "/Textures/WhiteSquareTexture.png";
        FRHIImageRef RHI = Import::Textures::CreateTextureFromImport(GRenderContext, SquareTexturePath.ToString(), false);

        ImTextureRef ImTex = ImGuiX::ToImTextureRef(RHI);

        SquareWhiteTexture.first = SquareTexturePath;
        
        SquareWhiteTexture.second = Memory::New<FEntry>();
        SquareWhiteTexture.second->Name = SquareTexturePath; 
        SquareWhiteTexture.second->RHIImage = RHI;
        SquareWhiteTexture.second->ImTexture = ImTex;
        SquareWhiteTexture.second->State.store(ETextureState::Ready, std::memory_order_release);
        Images.emplace(SquareTexturePath, SquareWhiteTexture.second);
    }

    FUITextureCache::~FUITextureCache()
    {
        while (HasImagesPendingLoad())
        {
            Threading::Sleep(1);
        }

        Memory::Delete(SquareWhiteTexture.second);
        SquareWhiteTexture.second = nullptr;
        Images.erase(SquareWhiteTexture.first);
        
        for (auto& Pair : Images)
        {
            Memory::Delete(Pair.second);
        }
        
        Images.clear();
    }

    FUITextureCache& FUITextureCache::Get()
    {
        return *GetEngineSystem<FRenderManager>().GetTextureCache();
    }

    FRHIImageRef FUITextureCache::GetImage(const FName& Path)
    {
        FScopeLock Lock(Mutex);
        FEntry* Entry = GetOrCreateGroup(Path);
        Entry->LastUseFrame.store(GEngine->GetUpdateContext().GetFrame(), std::memory_order_relaxed);
        return Entry ? Entry->RHIImage : nullptr;
    }

    ImTextureRef FUITextureCache::GetImTexture(const FName& Path)
    {
        FScopeLock Lock(Mutex);
        FEntry* Entry = GetOrCreateGroup(Path);
        Entry->LastUseFrame.store(GEngine->GetUpdateContext().GetFrame(), std::memory_order_relaxed);
        return Entry ? Entry->ImTexture : ImTextureRef();
    }

    void FUITextureCache::StartFrame()
    {
        SquareWhiteTexture.second->LastUseFrame = GEngine->GetUpdateContext().GetFrame();
    }

    void FUITextureCache::EndFrame()
    {
        LUMINA_PROFILE_SCOPE();

        uint64 CurrentFrame = GEngine->GetUpdateContext().GetFrame();

        FScopeLock Lock(Mutex);
        eastl::erase_if(Images, [CurrentFrame](auto& pair) -> bool
        {
            FEntry* Entry = pair.second;
            if (Entry->State.load(std::memory_order_acquire) == ETextureState::Ready)
            {
                uint64 LastUse = Entry->LastUseFrame.load(std::memory_order_acquire);
                if (CurrentFrame - LastUse > 1000)
                {
                    GEngine->GetEngineSubsystem<FRenderManager>()->GetImGuiRenderer()->DestroyImTexture(Entry->ImTexture);
                    Memory::Delete(Entry);
                    return true;
                }
            }
            return false;
        });
    }

    bool FUITextureCache::HasImagesPendingLoad() const
    {
        for (auto& [Name, Entry] : Images)
        {
            if (Entry->State.load() == ETextureState::Loading)
            {
                return true;
            }
        }

        return false;
    }
    
    FUITextureCache::FEntry* FUITextureCache::GetOrCreateGroup(const FName& PathName)
    {
        LUMINA_PROFILE_SCOPE();
        
        auto Iter = Images.find(PathName);
        if (Iter != Images.end())
        {
            FEntry* Entry = Iter->second;
            if (Entry->State.load(std::memory_order_acquire) == ETextureState::Ready)
            {
                return Entry;
            }
            
            return SquareWhiteTexture.second;
        }
    
        FEntry* NewEntry = Memory::New<FEntry>();
        NewEntry->Name = PathName;
        NewEntry->State.store(ETextureState::Empty, std::memory_order_release);
        
        NewEntry->RHIImage = nullptr;
        NewEntry->ImTexture = {};
    
        auto [InsertedIter, Inserted] = Images.try_emplace(PathName, NewEntry);
        FEntry* Entry = InsertedIter->second;
        
        if (!Inserted) // Another thread inserted it first
        {
            Memory::Delete(NewEntry);
    
            if (Entry->State.load(std::memory_order_acquire) == ETextureState::Ready)
            {
                return Entry;
            }
            
            return SquareWhiteTexture.second;
        }
    
        ETextureState Expected = ETextureState::Empty;
        if (Entry->State.compare_exchange_strong(Expected, ETextureState::Loading, std::memory_order_acq_rel))
        {
            Task::AsyncTask(1, 1, [this, Entry, PathName](uint32, uint32, uint32)
            {
                FScopeLock Lock(Mutex);

                FString PathString = PathName.ToString();
                FRHIImageRef NewImage = Import::Textures::CreateTextureFromImport(GRenderContext, PathString, false);
                ImTextureRef NewImTexture = ImGuiX::ToImTextureRef(NewImage);
                
                Entry->RHIImage = NewImage;
                Entry->ImTexture = NewImTexture;
                Entry->LastUseFrame.store(GEngine->GetUpdateContext().GetFrame(), std::memory_order_release);
                
                Entry->State.store(ETextureState::Ready, std::memory_order_release);
            });
        }
    
        return SquareWhiteTexture.second;
    }
}
