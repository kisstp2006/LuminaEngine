
#include "Material.h"

#include "Assets/AssetTypes/Textures/Texture.h"
#include "Core/Engine/Engine.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RHIGlobals.h"
#include "Renderer/RHIIncl.h"

namespace Lumina
{
    CMaterial::CMaterial()
    {
        MaterialType = EMaterialType::PBR;
        Memory::Memzero(&MaterialUniforms, sizeof(FMaterialUniforms));
    }

    void CMaterial::Serialize(FArchive& Ar)
    {
        CMaterialInterface::Serialize(Ar);
        Ar << VertexShaderBinaries;
        Ar << PixelShaderBinaries;
    }

    void CMaterial::PostLoad()
    {
        FRHICommandListRef CommandList = GRenderContext->GetCommandList(ECommandQueue::Graphics);
        FShaderHeader Header;
        
        if (!VertexShaderBinaries.empty() && !PixelShaderBinaries.empty())
        {

            Header.DebugName = GetQualifiedName().ToString() + "_VertexShader";
            Header.Hash = Hash::GetHash64(VertexShaderBinaries.data(), VertexShaderBinaries.size() * sizeof(uint32));
            Header.Binaries = VertexShaderBinaries;
            VertexShader = GRenderContext->CreateVertexShader(Header);
            
            Header.DebugName = GetQualifiedName().ToString() + "_PixelShader";
            Header.Hash = Hash::GetHash64(PixelShaderBinaries.data(), PixelShaderBinaries.size() * sizeof(uint32));
            Header.Binaries = PixelShaderBinaries;
            PixelShader = GRenderContext->CreatePixelShader(Header);

            FRHIBufferDesc BufferDesc;
            BufferDesc.DebugName = GetName().ToString() + "Material Uniforms";
            BufferDesc.Size = sizeof(FMaterialUniforms);
            BufferDesc.InitialState = EResourceStates::ConstantBuffer;
            BufferDesc.bKeepInitialState = true;
            BufferDesc.Usage.SetFlag(BUF_UniformBuffer);
            UniformBuffer = GRenderContext->CreateBuffer(BufferDesc);
        
            Memory::Memzero(&MaterialUniforms, sizeof(FMaterialUniforms));
        
            CommandList->WriteBuffer(UniformBuffer, &MaterialUniforms, 0, sizeof(FMaterialUniforms));

            FBindingSetDesc SetDesc;
            SetDesc.AddItem(FBindingSetItem::BufferCBV(0, UniformBuffer));

            uint32 Index = 1;
            for (CTexture* Binding : Textures)
            {
                FRHIImageRef Image = Binding->GetRHIRef();
                SetDesc.AddItem(FBindingSetItem::TextureSRV(Index, Image));
                Index++;
            }
        
            TBitFlags<ERHIShaderType> Visibility;
            Visibility.SetMultipleFlags(ERHIShaderType::Vertex, ERHIShaderType::Fragment);
            GRenderContext->CreateBindingSetAndLayout(Visibility, 0, SetDesc, BindingLayout, BindingSet);

            SetReadyForRender(true);
        }
    }

    void CMaterial::OnDestroy()
    {
        CMaterialInterface::OnDestroy();
    }

    bool CMaterial::SetScalarValue(const FName& Name, const float Value)
    {
        auto* Itr = eastl::find_if(Parameters.begin(), Parameters.end(), [Name](const FMaterialParameter& Param)
        {
           return Param.ParameterName == Name && Param.Type == EMaterialParameterType::Scalar; 
        });

        if (Itr != Parameters.end())
        {
            const FMaterialParameter& Param = *Itr;

            int CecIndex = Param.Index / 4;
            int ComponentIndex = Param.Index % 4;
            
            MaterialUniforms.Scalars[CecIndex][ComponentIndex] = Value;
            return true;
        }
        else
        {
            LOG_ERROR("Failed to find material scalar parameter {}", Name);
        }

        return false;
    }

    bool CMaterial::SetVectorValue(const FName& Name, const glm::vec4& Value)
    {
        auto* Itr = eastl::find_if(Parameters.begin(), Parameters.end(), [Name](const FMaterialParameter& Param)
        {
           return Param.ParameterName == Name && Param.Type == EMaterialParameterType::Vector; 
        });

        if (Itr != Parameters.end())
        {
            const FMaterialParameter& Param = *Itr;
            MaterialUniforms.Vectors[Param.Index] = Value;
            return true;
        }
        else
        {
            LOG_ERROR("Failed to find material vector parameter {}", Name);
        }

        return false;
    }

    bool CMaterial::GetParameterValue(EMaterialParameterType Type, const FName& Name, FMaterialParameter& Param)
    {
        Param = {};
        auto* Itr = eastl::find_if(Parameters.begin(), Parameters.end(), [Type, Name](const FMaterialParameter& Param)
        {
           return Param.ParameterName == Name && Param.Type == Type; 
        });

        if (Itr != Parameters.end())
        {
            Param = *Itr;
            return true;
        }
        
        return false;
    }

    CMaterial* CMaterial::GetMaterial() const
    {
        return const_cast<CMaterial*>(this);
    }

    FRHIBindingSetRef CMaterial::GetBindingSet() const
    {
        return BindingSet;
    }

    FRHIBindingLayoutRef CMaterial::GetBindingLayout() const
    {
        return BindingLayout; 
    }

    FRHIVertexShaderRef CMaterial::GetVertexShader() const
    {
        return VertexShader;
    }

    FRHIPixelShaderRef CMaterial::GetPixelShader() const
    {
        return PixelShader;
    }
}
