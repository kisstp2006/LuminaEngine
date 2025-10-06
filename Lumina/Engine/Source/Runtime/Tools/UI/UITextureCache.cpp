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
        FEntry* Entry = GetOrCreateGroup(Path);
        Entry->LastUseFrame.store(GEngine->GetUpdateContext().GetFrame(), std::memory_order_relaxed);
        return Entry ? Entry->RHIImage : nullptr;
    }

    ImTextureRef FUITextureCache::GetImTexture(const FName& Path)
    {
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

        auto [InsertedIter, Inserted] = Images.try_emplace(PathName, NewEntry);
        FEntry* Entry = InsertedIter->second;
        Entries.push_back(NewEntry);

        if (!Inserted) // Another thread inserted it first
        {
            Entries.pop_back();
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
            Task::AsyncTask(1, [Entry, PathName](uint32, uint32, uint32)
            {
                FString PathString = PathName.ToString();
                Entry->RHIImage = Import::Textures::CreateTextureFromImport(GRenderContext, PathString, false);
                Entry->ImTexture = ImGuiX::ToImTextureRef(Entry->RHIImage);

                Entry->State.store(ETextureState::Ready, std::memory_order_release);
            });
        }

        return SquareWhiteTexture.second;
    }
}
