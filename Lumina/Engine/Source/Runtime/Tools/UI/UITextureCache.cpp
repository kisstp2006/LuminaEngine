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
        SquareWhiteTexture.second->State = ETextureState::Ready;
        
        Images.emplace(SquareTexturePath, SquareWhiteTexture.second);
    }
    
    FUITextureCache::~FUITextureCache()
    {
        // Wait for pending loads
        while (true)
        {
            {
                FScopeLock Lock(Mutex);
                if (!HasImagesPendingLoad_Internal())
                    break;
            }
            Threading::Sleep(1);
        }
    
        FScopeLock Lock(Mutex);
        
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
        FEntry* Entry = GetOrCreateGroup_Internal(Path);
        if (Entry)
        {
            Entry->LastUseFrame = GEngine->GetUpdateContext().GetFrame();
            return Entry->RHIImage;
        }
        return nullptr;
    }
    
    ImTextureRef FUITextureCache::GetImTexture(const FName& Path)
    {
        FScopeLock Lock(Mutex);
        FEntry* Entry = GetOrCreateGroup_Internal(Path);
        if (Entry)
        {
            Entry->LastUseFrame = GEngine->GetUpdateContext().GetFrame();
            return Entry->ImTexture;
        }
        return ImTextureRef();
    }
    
    void FUITextureCache::StartFrame()
    {
        FScopeLock Lock(Mutex);
        SquareWhiteTexture.second->LastUseFrame = GEngine->GetUpdateContext().GetFrame();
    }
    
    void FUITextureCache::EndFrame()
    {
        LUMINA_PROFILE_SCOPE();
    
        FScopeLock Lock(Mutex);
        
        uint64 CurrentFrame = GEngine->GetUpdateContext().GetFrame();
    
        eastl::erase_if(Images, [this, CurrentFrame](auto& pair) -> bool
        {
            FEntry* Entry = pair.second;
            
            // Don't delete the square white texture
            if (Entry == SquareWhiteTexture.second)
                return false;
                
            if (Entry->State == ETextureState::Ready)
            {
                uint64 LastUse = Entry->LastUseFrame;
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
        FScopeLock Lock(Mutex);
        return HasImagesPendingLoad_Internal();
    }
    
    bool FUITextureCache::HasImagesPendingLoad_Internal() const
    {
        for (auto& [Name, Entry] : Images)
        {
            if (Entry->State == ETextureState::Loading)
            {
                return true;
            }
        }
        return false;
    }
    
    FUITextureCache::FEntry* FUITextureCache::GetOrCreateGroup_Internal(const FName& PathName)
    {
        LUMINA_PROFILE_SCOPE();
        
        // Try to find existing entry
        auto Iter = Images.find(PathName);
        if (Iter != Images.end())
        {
            FEntry* Entry = Iter->second;
            if (Entry->State == ETextureState::Ready)
            {
                return Entry;
            }
            
            // Still loading, return placeholder
            return SquareWhiteTexture.second;
        }
    
        // Create new entry
        FEntry* NewEntry = Memory::New<FEntry>();
        NewEntry->Name = PathName;
        NewEntry->State = ETextureState::Loading;
        NewEntry->RHIImage = nullptr;
        NewEntry->ImTexture = {};
        NewEntry->LastUseFrame = GEngine->GetUpdateContext().GetFrame();
    
        Images.emplace(PathName, NewEntry);
        
        // Start async load (captures Entry pointer which is now stable in the map)
        Task::AsyncTask(1, 1, [this, NewEntry, PathName](uint32, uint32, uint32)
        {
            FString PathString = PathName.ToString();
            FRHIImageRef NewImage = Import::Textures::CreateTextureFromImport(GRenderContext, PathString, false);
            ImTextureRef NewImTexture = ImGuiX::ToImTextureRef(NewImage);
            
            {
                FScopeLock Lock(Mutex);
                
                auto Iter = Images.find(PathName);
                if (Iter != Images.end() && Iter->second == NewEntry)
                {
                    NewEntry->RHIImage = NewImage;
                    NewEntry->ImTexture = NewImTexture;
                    NewEntry->State = ETextureState::Ready;
                }
                else
                {
                    // Entry was removed, clean up the texture we just loaded
                    GEngine->GetEngineSubsystem<FRenderManager>()->GetImGuiRenderer()->DestroyImTexture(NewImTexture);
                }
            }
        });
    
        // Return placeholder while loading
        return SquareWhiteTexture.second;
    }
}
