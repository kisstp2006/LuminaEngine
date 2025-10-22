#include "TextureEditorTool.h"

#include "Assets/AssetTypes/Textures/Texture.h"
#include "Core/Object/Cast.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderManager.h"
#include "Tools/UI/ImGui/ImGuiFonts.h"
#include "Tools/UI/ImGui/ImGuiRenderer.h"

namespace Lumina
{
    const char* TexturePreviewName           = "TexturePreview";
    const char* TexturePropertiesName        = "TextureProperties";

    void FTextureEditorTool::OnInitialize()
    {
        // === Texture Preview Window ===
        CreateToolWindow(TexturePreviewName, [this](const FUpdateContext& Cxt, bool bFocused)
        {
            CTexture* Texture = Cast<CTexture>(Asset.Get());
            if (!Texture)
            {
                return;
            }

            FRenderManager* RenderManager = Cxt.GetSubsystem<FRenderManager>();
            ImTextureID TextureID = RenderManager->GetImGuiRenderer()->GetOrCreateImTexture(Texture->RHIImage);

            const FRHIImageDesc& ImageDesc = Texture->ImageDescription;
            ImVec2 WindowSize = ImGui::GetContentRegionAvail();
            ImVec2 WindowPos = ImGui::GetCursorScreenPos();

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
            {
                float Wheel = ImGui::GetIO().MouseWheel;
                if (Wheel != 0.0f)
                {
                    float ZoomSpeed = ImGui::GetIO().KeyCtrl ? 0.025f : 0.25f;
                    ZoomFactor += Wheel * ZoomSpeed;
                    ZoomFactor = ImClamp(ZoomFactor, 0.1f, 20.0f);
                }

                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                {
                    ImVec2 MouseDelta = ImGui::GetIO().MouseDelta;
                    PanOffset.x += MouseDelta.x;
                    PanOffset.y += MouseDelta.y;
                }

                // Reset view on double-click
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    ZoomFactor = 1.0f;
                    PanOffset = ImVec2(0, 0);
                }
            }

            ImDrawList* DrawList = ImGui::GetWindowDrawList();
            const int CheckerSize = 16;
            const ImU32 CheckerColor1 = IM_COL32(50, 50, 55, 255);
            const ImU32 CheckerColor2 = IM_COL32(40, 40, 45, 255);

            for (int y = 0; y < (int)WindowSize.y; y += CheckerSize)
            {
                for (int x = 0; x < (int)WindowSize.x; x += CheckerSize)
                {
                    bool isEven = ((x / CheckerSize) + (y / CheckerSize)) % 2 == 0;
                    ImU32 color = isEven ? CheckerColor1 : CheckerColor2;
                    DrawList->AddRectFilled(ImVec2(WindowPos.x + x, WindowPos.y + y), ImVec2(WindowPos.x + x + CheckerSize, WindowPos.y + y + CheckerSize), color);
                }
            }

            ImVec2 TextureSize = ImVec2((float)ImageDesc.Extent.X, (float)ImageDesc.Extent.Y);
            ImVec2 ScaledSize = ImVec2(TextureSize.x * ZoomFactor, TextureSize.y * ZoomFactor);
            ImVec2 CenterPos = ImVec2(WindowPos.x + (WindowSize.x - ScaledSize.x) * 0.5f + PanOffset.x,WindowPos.y + (WindowSize.y - ScaledSize.y) * 0.5f + PanOffset.y);

            // Draw border around texture
            DrawList->AddRect(
                ImVec2(CenterPos.x - 1, CenterPos.y - 1),
                ImVec2(CenterPos.x + ScaledSize.x + 1, CenterPos.y + ScaledSize.y + 1),
                IM_COL32(100, 100, 120, 255),
                0.0f,
                0,
                2.0f
            );

            DrawList->AddImage(
                TextureID,
                CenterPos,
                ImVec2(CenterPos.x + ScaledSize.x, CenterPos.y + ScaledSize.y),
                ImVec2(0, 0),
                ImVec2(1, 1),
                IM_COL32(255, 255, 255, 255)
            );

            ImVec2 MousePos = ImGui::GetMousePos();
            bool isOverTexture = MousePos.x >= CenterPos.x && MousePos.x <= CenterPos.x + ScaledSize.x &&
                MousePos.y >= CenterPos.y && MousePos.y <= CenterPos.y + ScaledSize.y;

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && isOverTexture)
            {
                float NormX = (MousePos.x - CenterPos.x) / ScaledSize.x;
                float NormY = (MousePos.y - CenterPos.y) / ScaledSize.y;
                int PixelX = (int)(NormX * TextureSize.x);
                int PixelY = (int)(NormY * TextureSize.y);

                // Draw crosshair
                const ImU32 CrosshairColor = IM_COL32(255, 255, 255, 180);
                DrawList->AddLine(ImVec2(CenterPos.x, MousePos.y), ImVec2(CenterPos.x + ScaledSize.x, MousePos.y), CrosshairColor);
                DrawList->AddLine(ImVec2(MousePos.x, CenterPos.y), ImVec2(MousePos.x, CenterPos.y + ScaledSize.y), CrosshairColor);

                // Pixel info tooltip
                ImGui::BeginTooltip();
                ImGui::Text("Pixel: [%d, %d]", PixelX, PixelY);
                ImGui::Text("UV: [%.3f, %.3f]", NormX, NormY);
                ImGui::EndTooltip();
            }

            ImGui::SetCursorScreenPos(ImVec2(WindowPos.x + 10, WindowPos.y + 10));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 20, 25, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));

            if (ImGui::BeginChild("##Toolbar", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar))
            {
                // Mip level selector
                if (ImageDesc.NumMips > 1)
                {
                    ImGui::Text("Mip Level:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100);
                    ImGui::SliderInt("##MipLevel", &CurrentMipLevel, 0, ImageDesc.NumMips - 1);
                }

            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();

            ImGui::SetCursorScreenPos(ImVec2(WindowPos.x + 10, WindowPos.y + WindowSize.y - 35));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 20, 25, 230));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));

            if (ImGui::BeginChild("##InfoBar", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs))
            {
                ImGui::Text("%.0f%% | %dx%d | Pan: [%.0f, %.0f]",
                    ZoomFactor * 100.0f,
                    (int)TextureSize.x,
                    (int)TextureSize.y,
                    PanOffset.x,
                    PanOffset.y
                );
                ImGui::SameLine(0, 20);
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "RMB/MMB: Pan | Scroll: Zoom | Ctrl+Scroll: Fine Zoom | Double-Click: Reset");
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        });
    
        CreateToolWindow(TexturePropertiesName, [this](const FUpdateContext& Cxt, bool bFocused)
        {
            CTexture* Texture = Cast<CTexture>(Asset.Get());
            if (!Texture)
            {
                return;
            }

            const FRHIImageDesc& ImageDesc = Texture->ImageDescription;
    
            // === Header Section ===
            ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Large);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", Texture->GetName().c_str());
            ImGuiX::Font::PopFont();
            
            ImGui::Spacing();
    
            // === Texture Information ===
            ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Large);
            ImGui::SeparatorText("Texture Information");
            ImGuiX::Font::PopFont();
            
            ImGui::Spacing();
    
            if (ImGui::BeginTable("##TextureInfo", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
    
                auto PropertyRow = [](const char* label, const FString& value, const ImVec4* color = nullptr)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(label);
                    ImGui::TableSetColumnIndex(1);
                    if (color) ImGui::PushStyleColor(ImGuiCol_Text, *color);
                    ImGui::TextUnformatted(value.c_str());
                    if (color) ImGui::PopStyleColor();
                };
    
                // Dimensions
                FString dimensionStr;
                switch (ImageDesc.Dimension)
                {
                    case EImageDimension::Texture2D: dimensionStr = "2D Texture"; break;
                    case EImageDimension::Texture3D: dimensionStr = "3D Volume"; break;
                    case EImageDimension::TextureCube: dimensionStr = "Cubemap"; break;
                    default: dimensionStr = "Unknown"; break;
                }
                
                ImVec4 dimensionColor(0.5f, 0.9f, 0.5f, 1.0f);
                PropertyRow("Type", dimensionStr, &dimensionColor);
                PropertyRow("Resolution", eastl::to_string(ImageDesc.Extent.X) + " x " + eastl::to_string(ImageDesc.Extent.Y));
                
                if (ImageDesc.Depth > 1)
                {
                    PropertyRow("Depth", eastl::to_string(ImageDesc.Depth));
                }
                
                if (ImageDesc.ArraySize > 1)
                {
                    ImVec4 arrayColor(0.9f, 0.7f, 0.4f, 1.0f);
                    PropertyRow("Array Size", eastl::to_string(ImageDesc.ArraySize), &arrayColor);
                }


    
                // Format information
				const FFormatInfo& FormatInfo = GetFormatInfo(ImageDesc.Format);
                PropertyRow("Pixel Format", FString(FormatInfo.Name));
                PropertyRow("Mip Levels", eastl::to_string(ImageDesc.NumMips));
                
                if (ImageDesc.NumSamples > 1)
                {
                    ImVec4 msaaColor(0.9f, 0.5f, 0.9f, 1.0f);
                    PropertyRow("MSAA Samples", eastl::to_string(ImageDesc.NumSamples) + "x", &msaaColor);
                }
    
                // Memory calculation
                size_t pixelCount = Texture->Pixels.size();
                size_t memorySizeKB = pixelCount / 1024;
                size_t memorySizeMB = memorySizeKB / 1024;
                
                FString memoryStr;
                ImVec4 memoryColor(0.7f, 1.0f, 0.7f, 1.0f);
                
                if (memorySizeMB > 0)
                {
                    memoryStr = eastl::to_string(memorySizeMB) + " MB";
                    if (memorySizeMB > 10) memoryColor = ImVec4(1.0f, 0.7f, 0.3f, 1.0f);
                    if (memorySizeMB > 50) memoryColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                }
                else
                {
                    memoryStr = eastl::to_string(memorySizeKB) + " KB";
                }
                
                PropertyRow("Memory Size", memoryStr, &memoryColor);
                PropertyRow("Pixel Count", eastl::to_string(pixelCount));
    
                ImGui::EndTable();
            }
    
            ImGui::Spacing();
            ImGui::Spacing();
    
            // === Image Flags ===
            ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Large);
            ImGui::SeparatorText("Image Capabilities");
            ImGuiX::Font::PopFont();
            
            ImGui::Spacing();
    
            struct FlagInfo
            {
                EImageCreateFlags Flag;
                const char* Name;
                const char* Description;
                ImVec4 Color;
            };
    
            FlagInfo flags[] = 
            {
                { EImageCreateFlags::ShaderResource, "Shader Resource", "Can be sampled in shaders", ImVec4(0.5f, 0.8f, 1.0f, 1.0f) },
                { EImageCreateFlags::RenderTarget, "Render Target", "Can be rendered to", ImVec4(1.0f, 0.6f, 0.3f, 1.0f) },
                { EImageCreateFlags::DepthStencil, "Depth/Stencil", "Used for depth testing", ImVec4(0.6f, 0.4f, 0.9f, 1.0f) },
                { EImageCreateFlags::Storage, "Storage", "Supports storage operations", ImVec4(0.9f, 0.9f, 0.3f, 1.0f) },
                { EImageCreateFlags::UnorderedAccess, "Unordered Access", "UAV/RWTexture support", ImVec4(1.0f, 0.4f, 0.6f, 1.0f) },
                { EImageCreateFlags::InputAttachment, "Input Attachment", "Can be used as input", ImVec4(0.4f, 0.9f, 0.6f, 1.0f) },
                { EImageCreateFlags::CubeCompatible, "Cube Compatible", "Cubemap compatible", ImVec4(0.7f, 0.5f, 1.0f, 1.0f) },
                { EImageCreateFlags::Aliasable, "Aliasable", "Memory can be aliased", ImVec4(0.8f, 0.8f, 0.8f, 1.0f) }
            };
    
            bool anyFlagSet = false;
            for (const auto& flagInfo : flags)
            {
                if (ImageDesc.Flags.IsFlagSet(flagInfo.Flag))
                {
                    anyFlagSet = true;
                    ImGui::PushStyleColor(ImGuiCol_Text, flagInfo.Color);
                    ImGui::Bullet();
                    ImGui::SameLine();
                    ImGui::Text("%s", flagInfo.Name);
                    ImGui::PopStyleColor();
                    
                    ImGui::SameLine(200);
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "- %s", flagInfo.Description);
                }
            }
    
            if (!anyFlagSet)
            {
                ImGui::TextDisabled("No special flags set");
            }
    
            ImGui::Spacing();
            ImGui::Spacing();
    
            // === Statistics ===
            ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Large);
            ImGui::SeparatorText("Statistics");
            ImGuiX::Font::PopFont();
            
            ImGui::Spacing();
    
            float aspectRatio = (float)ImageDesc.Extent.X / (float)ImageDesc.Extent.Y;
            uint32 totalTexels = ImageDesc.Extent.X * ImageDesc.Extent.Y * ImageDesc.Depth * ImageDesc.ArraySize;
            
            if (ImGui::BeginTable("##Stats", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);
                
                auto StatRow = [](const char* label, const FString& value)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", label);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(value.c_str());
                };
                
                char aspectStr[32];
                snprintf(aspectStr, sizeof(aspectStr), "%.3f (%.2f:1)", aspectRatio, aspectRatio);
                StatRow("Aspect Ratio:", aspectStr);
                
                StatRow("Total Texels:", eastl::to_string(totalTexels));
                
                if (ImageDesc.NumMips > 1)
                {
                    uint32 mipChainSize = 0;
                    for (int i = 0; i < ImageDesc.NumMips; ++i)
                    {
                        uint32 mipWidth = eastl::max<uint32>(1u, ImageDesc.Extent.X >> i);
                        uint32 mipHeight = eastl::max<uint32>(1u, ImageDesc.Extent.Y >> i);
                        mipChainSize += mipWidth * mipHeight;
                    }
                    StatRow("Mip Chain Texels:", eastl::to_string(mipChainSize));
                }
    
                ImGui::EndTable();
            }
    
            ImGui::Spacing();
            ImGui::Spacing();
    
            // === Quick Actions ===
            ImGuiX::Font::PushFont(ImGuiX::Font::EFont::Large);
            ImGui::SeparatorText("Actions");
            ImGuiX::Font::PopFont();
            
            ImGui::Spacing();
    
            if (ImGui::Button("Export to File...", ImVec2(-1, 0)))
            {
                // TODO: Implement export
            }
    
            if (ImGui::Button("Generate Mipmaps", ImVec2(-1, 0)))
            {
                // TODO: Implement mipmap generation
            }
    
            if (ImGui::Button("Analyze Color Distribution", ImVec2(-1, 0)))
            {
                // TODO: Implement analysis
            }
        });
    }

    void FTextureEditorTool::OnDeinitialize(const FUpdateContext& UpdateContext)
    {
    }

    void FTextureEditorTool::OnAssetLoadFinished()
    {
    }

    void FTextureEditorTool::DrawToolMenu(const FUpdateContext& UpdateContext)
    {
        FAssetEditorTool::DrawToolMenu(UpdateContext);
    }

    void FTextureEditorTool::InitializeDockingLayout(ImGuiID InDockspaceID, const ImVec2& InDockspaceSize) const
    {
        ImGuiID leftDockID = 0, rightDockID = 0, bottomDockID = 0;

        ImGui::DockBuilderSplitNode(InDockspaceID, ImGuiDir_Right, 0.3f, &rightDockID, &leftDockID);

        ImGui::DockBuilderSplitNode(InDockspaceID, ImGuiDir_Down, 0.3f, &bottomDockID, &InDockspaceID);

        ImGui::DockBuilderDockWindow(GetToolWindowName(TexturePreviewName).c_str(), leftDockID);
        ImGui::DockBuilderDockWindow(GetToolWindowName(TexturePropertiesName).c_str(), rightDockID);

    }
}
