#include "MaterialCompiler.h"

#include "Assets/AssetTypes/Textures/Texture.h"
#include "Core/Object/Cast.h"
#include "Nodes/MaterialNodeExpression.h"
#include "Paths/Paths.h"
#include "Platform/Filesystem/FileHelper.h"
#include "UI/Tools/NodeGraph/EdGraphNode.h"

namespace Lumina
{
    FMaterialCompiler::FMaterialCompiler()
    {
        ShaderChunks.reserve(2000);
    }

    FString FMaterialCompiler::BuildTree(SIZE_T& StartReplacement, SIZE_T& EndReplacement)
    {
        FString FragmentPath = Paths::GetEngineResourceDirectory() + "/MaterialShader/GeometryPass.frag";

        FString LoadedString;
        if (!FileHelper::LoadFileIntoString(LoadedString, FragmentPath))
        {
            LOG_ERROR("Failed to find GeometryPass.frag!");
            return FString();
        }

        const char* Token = "$MATERIAL_INPUTS";
        size_t Pos = LoadedString.find(Token);

        if (Pos != FString::npos)
        {
            StartReplacement = Pos;
            LoadedString.replace(Pos, strlen(Token), ShaderChunks);
            EndReplacement = Pos + ShaderChunks.length();
        }
        else
        {
            LOG_ERROR("Missing [$MATERIAL_INPUTS] in base shader!");
            return FString();
        }
        
        return LoadedString;
    }

    void FMaterialCompiler::ValidateConnections(CMaterialInput* A, CMaterialInput* B)
    {
        // Could add type compatibility checking here
    }

    FString FMaterialCompiler::GetVectorType(EMaterialValueType Type) const
    {
        switch (Type)
        {
            case EMaterialValueType::Float: return "float";
            case EMaterialValueType::Float2: return "vec2";
            case EMaterialValueType::Float3: return "vec3";
            case EMaterialValueType::Float4: return "vec4";
            default: return "float";
        }
    }

    FString FMaterialCompiler::GetVectorType(int32 ComponentCount) const
    {
        switch (ComponentCount)
        {
            case 1: return "float";
            case 2: return "vec2";
            case 3: return "vec3";
            case 4: return "vec4";
            default: return "float";
        }
    }

    int32 FMaterialCompiler::GetComponentCount(EComponentMask Mask) const
    {
        int32 Count = 0;
        if (static_cast<uint32>(Mask) & static_cast<uint32>(EComponentMask::R))
        {
            Count++;
        }
        if (static_cast<uint32>(Mask) & static_cast<uint32>(EComponentMask::G))
        {
            Count++;
        }
        if (static_cast<uint32>(Mask) & static_cast<uint32>(EComponentMask::B))
        {
            Count++;
        }
        if (static_cast<uint32>(Mask) & static_cast<uint32>(EComponentMask::A))
        {
            Count++;
        }
        return Count;
    }

    int32 FMaterialCompiler::GetComponentCount(EMaterialValueType Type) const
    {
        switch (Type)
        {
            case EMaterialValueType::Float: return 1;
            case EMaterialValueType::Float2: return 2;
            case EMaterialValueType::Float3: return 3;
            case EMaterialValueType::Float4: return 4;
            default: return 1;
        }
    }

    EMaterialValueType FMaterialCompiler::GetTypeFromComponentCount(int32 Count) const
    {
        switch (Count)
        {
            case 1: return EMaterialValueType::Float;
            case 2: return EMaterialValueType::Float2;
            case 3: return EMaterialValueType::Float3;
            case 4: return EMaterialValueType::Float4;
            default: return EMaterialValueType::Float;
        }
    }

    void FMaterialCompiler::RegisterNodeOutput(const FString& NodeName, EMaterialValueType Type, EComponentMask Mask)
    {
        FNodeOutputInfo Info;
        Info.Type = Type;
        Info.Mask = Mask;
        Info.NodeName = NodeName;
        NodeOutputs[NodeName] = Info;
    }

    FMaterialCompiler::FNodeOutputInfo FMaterialCompiler::GetNodeOutputInfo(const FString& NodeName) const
    {
        auto It = NodeOutputs.find(NodeName);
        if (It != NodeOutputs.end())
        {
            return It->second;
        }
        
        FNodeOutputInfo DefaultInfo;
        DefaultInfo.Type = EMaterialValueType::Float4;
        DefaultInfo.Mask = EComponentMask::RGBA;
        DefaultInfo.NodeName = NodeName;
        return DefaultInfo;
    }

    FMaterialCompiler::FInputValue FMaterialCompiler::GetTypedInputValue(CMaterialInput* Input, float DefaultValue)
    {
        return GetTypedInputValue(Input, eastl::to_string(DefaultValue));
    }

    FMaterialCompiler::FInputValue FMaterialCompiler::GetTypedInputValue(CMaterialInput* Input, const FString& DefaultValueStr)
    {
        FInputValue Result;
        
        if (Input->HasConnection())
        {
            CMaterialOutput* Conn = Input->GetConnection<CMaterialOutput>(0);
            FString NodeName = Conn->GetOwningNode()->GetNodeFullName();
            FNodeOutputInfo Info = GetNodeOutputInfo(NodeName);
            
            Result.Type = Info.Type;
            Result.ComponentCount = GetComponentCount(Info.Type);
            
            FString Swizzle = GetSwizzleForMask(Conn->GetComponentMask());
            
            // If swizzle is applied, determine the resulting type
            if (!Swizzle.empty())
            {
                int32 SwizzleCount = Swizzle.length() - 1; // -1 for the '.' character
                Result.ComponentCount = SwizzleCount;
                Result.Type = GetTypeFromComponentCount(SwizzleCount);
                Result.Value = NodeName + Swizzle;
            }
            else
            {
                Result.Value = NodeName;
            }
        }
        else
        {
            // No connection - use default
            Result.Type = EMaterialValueType::Float;
            Result.ComponentCount = 1;
            Result.Value = DefaultValueStr;
        }
        
        return Result;
    }

    EMaterialValueType FMaterialCompiler::DetermineResultType(EMaterialValueType A, EMaterialValueType B, bool IsComponentWise)
    {
        int32 CountA = GetComponentCount(A);
        int32 CountB = GetComponentCount(B);
        
        if (IsComponentWise)
        {
            // For component-wise ops (*, /, etc.), scalar broadcasts to match the other
            if (CountA == 1)
            {
                return B;
            }
            if (CountB == 1)
            {
                return A;
            }

            // Both are vectors - they should match or this is an error
            // For now, return the larger type
            return CountA >= CountB ? A : B;
        }
        else
        {
            // For operations like dot product that reduce dimensionality
            return EMaterialValueType::Float;
        }
    }

    void FMaterialCompiler::EmitBinaryOp(const FString& Op, CMaterialInput* A, CMaterialInput* B, float DefaultA, float DefaultB, bool IsComponentWise)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        
        FInputValue AValue = GetTypedInputValue(A, DefaultA);
        FInputValue BValue = GetTypedInputValue(B, DefaultB);
        
        // Determine result type
        EMaterialValueType ResultType = DetermineResultType(AValue.Type, BValue.Type, IsComponentWise);
        FString ResultTypeStr = GetVectorType(ResultType);
        
        // Check for invalid operations
        if (AValue.ComponentCount > 1 && BValue.ComponentCount > 1 && AValue.ComponentCount != BValue.ComponentCount)
        {
            FError Error;
            Error.ErrorNode         = A->GetOwningNode<CMaterialGraphNode>();
            Error.ErrorName         = "Type Mismatch";
            Error.ErrorDescription  = "Cannot perform " + Op + " between " + GetVectorType(AValue.Type) + " and " + GetVectorType(BValue.Type);
            AddError(Error);
            
            // Still generate code but with a warning comment
            ShaderChunks.append("// ERROR: Type mismatch\n");
        }
        
        // Emit the operation
        ShaderChunks.append(ResultTypeStr + " " + OwningNode + " = " + AValue.Value + " " + Op + " " + BValue.Value + ";\n");
        
        // Register the output
        RegisterNodeOutput(OwningNode, ResultType, EComponentMask::RGBA);
    }

    // ========================================================================
    // Parameter Definitions
    // ========================================================================

    void FMaterialCompiler::DefineFloatParameter(const FString& NodeID, const FName& ParamID, float Value)
    {
        if (ScalarParameters.find(ParamID) == ScalarParameters.end())
        {
            ScalarParameters[ParamID].Index = NumScalarParams++;
            ScalarParameters[ParamID].Value = Value;
        }

        FString IndexString = eastl::to_string(ScalarParameters[ParamID].Index);
        ShaderChunks.append("float " + NodeID + " = GetMaterialScalar(" + IndexString + ");\n");
        RegisterNodeOutput(NodeID, EMaterialValueType::Float, EComponentMask::R);
    }

    void FMaterialCompiler::DefineFloat2Parameter(const FString& NodeID, const FName& ParamID, float Value[2])
    {
        if (VectorParameters.find(ParamID) == VectorParameters.end())
        {
            VectorParameters[ParamID].Index = NumVectorParams++;
            VectorParameters[ParamID].Value = glm::vec4(Value[0], Value[1], 0.0f, 1.0f);
        }

        FString IndexString = eastl::to_string(VectorParameters[ParamID].Index);
        ShaderChunks.append("vec2 " + NodeID + " = GetMaterialVec4(" + IndexString + ").xy;\n");
        RegisterNodeOutput(NodeID, EMaterialValueType::Float2, EComponentMask::RG);
    }

    void FMaterialCompiler::DefineFloat3Parameter(const FString& NodeID, const FName& ParamID, float Value[3])
    {
        if (VectorParameters.find(ParamID) == VectorParameters.end())
        {
            VectorParameters[ParamID].Index = NumVectorParams++;
            VectorParameters[ParamID].Value = glm::vec4(Value[0], Value[1], Value[2], 1.0f);
        }
        
        FString IndexString = eastl::to_string(VectorParameters[ParamID].Index);
        ShaderChunks.append("vec3 " + NodeID + " = GetMaterialVec4(" + IndexString + ").xyz;\n");
        RegisterNodeOutput(NodeID, EMaterialValueType::Float3, EComponentMask::RGB);
    }

    void FMaterialCompiler::DefineFloat4Parameter(const FString& NodeID, const FName& ParamID, float Value[4])
    {
        if (VectorParameters.find(ParamID) == VectorParameters.end())
        {
            VectorParameters[ParamID].Index = NumVectorParams++;
            VectorParameters[ParamID].Value = glm::vec4(Value[0], Value[1], Value[2], Value[3]);
        }
        
        FString IndexString = eastl::to_string(VectorParameters[ParamID].Index);
        ShaderChunks.append("vec4 " + NodeID + " = GetMaterialVec4(" + IndexString + ");\n");
        RegisterNodeOutput(NodeID, EMaterialValueType::Float4, EComponentMask::RGBA);
    }

    // ========================================================================
    // Constant Definitions
    // ========================================================================

    void FMaterialCompiler::DefineConstantFloat(const FString& ID, float Value)
    {
        FString ValueString = eastl::to_string(Value);
        ShaderChunks.append("float " + ID + " = " + ValueString + ";\n");
        RegisterNodeOutput(ID, EMaterialValueType::Float, EComponentMask::R);
    }

    void FMaterialCompiler::DefineConstantFloat2(const FString& ID, float Value[2])
    {
        FString ValueStringX = eastl::to_string(Value[0]);
        FString ValueStringY = eastl::to_string(Value[1]);
        ShaderChunks.append("vec2 " + ID + " = vec2(" + ValueStringX + ", " + ValueStringY + ");\n");
        RegisterNodeOutput(ID, EMaterialValueType::Float2, EComponentMask::RG);
    }

    void FMaterialCompiler::DefineConstantFloat3(const FString& ID, float Value[3])
    {
        FString ValueStringX = eastl::to_string(Value[0]);
        FString ValueStringY = eastl::to_string(Value[1]);
        FString ValueStringZ = eastl::to_string(Value[2]);
        ShaderChunks.append("vec3 " + ID + " = vec3(" + ValueStringX + ", " + ValueStringY + ", " + ValueStringZ + ");\n");
        RegisterNodeOutput(ID, EMaterialValueType::Float3, EComponentMask::RGB);
    }

    void FMaterialCompiler::DefineConstantFloat4(const FString& ID, float Value[4])
    {
        FString ValueStringX = eastl::to_string(Value[0]);
        FString ValueStringY = eastl::to_string(Value[1]);
        FString ValueStringZ = eastl::to_string(Value[2]);
        FString ValueStringW = eastl::to_string(Value[3]);
        ShaderChunks.append("vec4 " + ID + " = vec4(" + ValueStringX + ", " + ValueStringY + ", " + 
                           ValueStringZ + ", " + ValueStringW + ");\n");
        RegisterNodeOutput(ID, EMaterialValueType::Float4, EComponentMask::RGBA);
    }

    // ========================================================================
    // Texture Operations
    // ========================================================================

    void FMaterialCompiler::DefineTextureSample(const FString& ID)
    {
        ShaderChunks.append("layout(set = 1, binding = " + eastl::to_string(BindingIndex) + 
                           ") uniform sampler2D " + ID + "_sample;\n");
        BindingIndex++;
    }

    void FMaterialCompiler::TextureSample(const FString& ID, CTexture* Texture, CMaterialInput* Input)
    {
        if (Texture == nullptr || Texture->RHIImage == nullptr)
        {
            return;
        }

        FInputValue UVValue = GetTypedInputValue(Input, "vec2(UV0)");
        
        // Ensure we have vec2 for UVs
        FString UVStr;
        if (UVValue.ComponentCount >= 2)
        {
            UVStr = UVValue.Value + ".xy";
        }
        else
        {
            UVStr = "vec2(" + UVValue.Value + ")";
        }
        
        ShaderChunks.append("vec4 " + ID + " = texture(" + ID + "_sample, " + UVStr + ");\n");
        RegisterNodeOutput(ID, EMaterialValueType::Float4, EComponentMask::RGBA);
        BoundImages.push_back(Texture);
    }

    // ========================================================================
    // Built-in Inputs
    // ========================================================================

    void FMaterialCompiler::NewLine()
    {
        ShaderChunks.append("\n");
    }

    void FMaterialCompiler::VertexNormal(const FString& ID)
    {
        ShaderChunks.append("vec3 " + ID + " = WorldNormal.xyz;\n");
        RegisterNodeOutput(ID, EMaterialValueType::Float3, EComponentMask::RGB);
    }

    void FMaterialCompiler::TexCoords(const FString& ID)
    {
        ShaderChunks.append("vec2 " + ID + " = UV0;\n");
        RegisterNodeOutput(ID, EMaterialValueType::Float2, EComponentMask::RG);
    }

    void FMaterialCompiler::WorldPos(const FString& ID)
    {
        ShaderChunks.append("vec3 " + ID + " = inModelMatrix[3].xyz;\n");
        RegisterNodeOutput(ID, EMaterialValueType::Float3, EComponentMask::RGB);
    }

    void FMaterialCompiler::CameraPos(const FString& ID)
    {
        ShaderChunks.append("vec3 " + ID + " = GetCameraPosition();\n");
        RegisterNodeOutput(ID, EMaterialValueType::Float3, EComponentMask::RGB);
    }

    void FMaterialCompiler::EntityID(const FString& ID)
    {
        ShaderChunks.append("float " + ID + " = float(EntityID);\n");
        RegisterNodeOutput(ID, EMaterialValueType::Float, EComponentMask::R);
    }

    void FMaterialCompiler::Time(const FString& ID)
    {
        ShaderChunks.append("float " + ID + " = GetTime();\n");
        RegisterNodeOutput(ID, EMaterialValueType::Float, EComponentMask::R);
    }

    // ========================================================================
    // Math Operations
    // ========================================================================

    void FMaterialCompiler::Multiply(CMaterialInput* A, CMaterialInput* B)
    {
        CMaterialExpression_Multiplication* Node = A->GetOwningNode<CMaterialExpression_Multiplication>();
        EmitBinaryOp("*", A, B, Node->ConstA, Node->ConstB, true);
    }

    void FMaterialCompiler::Divide(CMaterialInput* A, CMaterialInput* B)
    {
        CMaterialExpression_Division* Node = A->GetOwningNode<CMaterialExpression_Division>();
        EmitBinaryOp("/", A, B, Node->ConstA, Node->ConstB, true);
    }

    void FMaterialCompiler::Add(CMaterialInput* A, CMaterialInput* B)
    {
        CMaterialExpression_Addition* Node = A->GetOwningNode<CMaterialExpression_Addition>();
        EmitBinaryOp("+", A, B, Node->ConstA, Node->ConstB, true);
    }

    void FMaterialCompiler::Subtract(CMaterialInput* A, CMaterialInput* B)
    {
        CMaterialExpression_Subtraction* Node = A->GetOwningNode<CMaterialExpression_Subtraction>();
        EmitBinaryOp("-", A, B, Node->ConstA, Node->ConstB, true);
    }

    void FMaterialCompiler::Sin(CMaterialInput* A, CMaterialInput* B)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        CMaterialExpression_Sin* Node = A->GetOwningNode<CMaterialExpression_Sin>();

        FInputValue AValue = GetTypedInputValue(A, Node->ConstA);
        FString TypeStr = GetVectorType(AValue.Type);

        ShaderChunks.append(TypeStr + " " + OwningNode + " = sin(" + AValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, AValue.Type, EComponentMask::RGBA);
    }

    void FMaterialCompiler::Cos(CMaterialInput* A, CMaterialInput* B)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        CMaterialExpression_Cosin* Node = A->GetOwningNode<CMaterialExpression_Cosin>();

        FInputValue AValue = GetTypedInputValue(A, Node->ConstA);
        FString TypeStr = GetVectorType(AValue.Type);

        ShaderChunks.append(TypeStr + " " + OwningNode + " = cos(" + AValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, AValue.Type, EComponentMask::RGBA);
    }

    void FMaterialCompiler::Floor(CMaterialInput* A, CMaterialInput* B)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        CMaterialExpression_Floor* Node = A->GetOwningNode<CMaterialExpression_Floor>();

        FInputValue AValue = GetTypedInputValue(A, Node->ConstA);
        FString TypeStr = GetVectorType(AValue.Type);

        ShaderChunks.append(TypeStr + " " + OwningNode + " = floor(" + AValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, AValue.Type, EComponentMask::RGBA);
    }

    void FMaterialCompiler::Ceil(CMaterialInput* A, CMaterialInput* B)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        CMaterialExpression_Ceil* Node = A->GetOwningNode<CMaterialExpression_Ceil>();

        FInputValue AValue = GetTypedInputValue(A, Node->ConstA);
        FString TypeStr = GetVectorType(AValue.Type);

        ShaderChunks.append(TypeStr + " " + OwningNode + " = ceil(" + AValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, AValue.Type, EComponentMask::RGBA);
    }

    void FMaterialCompiler::Power(CMaterialInput* A, CMaterialInput* B)
    {
        CMaterialExpression_Power* Node = A->GetOwningNode<CMaterialExpression_Power>();
        
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        FInputValue AValue = GetTypedInputValue(A, Node->ConstA);
        FInputValue BValue = GetTypedInputValue(B, Node->ConstB);
        
        EMaterialValueType ResultType = DetermineResultType(AValue.Type, BValue.Type, true);
        FString TypeStr = GetVectorType(ResultType);

        // Check for invalid operations
        if (AValue.ComponentCount > 1 && BValue.ComponentCount > 1 && AValue.ComponentCount != BValue.ComponentCount)
        {
            FError Error;
            Error.ErrorNode         = A->GetOwningNode<CMaterialGraphNode>();
            Error.ErrorName         = "Type Mismatch";
            Error.ErrorDescription  = "Cannot perform Power between " + GetVectorType(AValue.Type) + " and " + GetVectorType(BValue.Type);
            AddError(Error);
            
            ShaderChunks.append("// ERROR: Type mismatch\n");
        }

        ShaderChunks.append(TypeStr + " " + OwningNode + " = pow(" + AValue.Value + ", " + BValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, ResultType, EComponentMask::RGBA);
    }

    void FMaterialCompiler::Mod(CMaterialInput* A, CMaterialInput* B)
    {
        CMaterialExpression_Mod* Node = A->GetOwningNode<CMaterialExpression_Mod>();

        //...
    }

    void FMaterialCompiler::Min(CMaterialInput* A, CMaterialInput* B)
    {
        CMaterialExpression_Min* Node = A->GetOwningNode<CMaterialExpression_Min>();
        
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        FInputValue AValue = GetTypedInputValue(A, Node->ConstA);
        FInputValue BValue = GetTypedInputValue(B, Node->ConstB);
        
        EMaterialValueType ResultType = DetermineResultType(AValue.Type, BValue.Type, true);
        FString TypeStr = GetVectorType(ResultType);

        // Check for invalid operations
        if (AValue.ComponentCount > 1 && BValue.ComponentCount > 1 && AValue.ComponentCount != BValue.ComponentCount)
        {
            FError Error;
            Error.ErrorNode         = A->GetOwningNode<CMaterialGraphNode>();
            Error.ErrorName         = "Type Mismatch";
            Error.ErrorDescription  = "Cannot perform Power between " + GetVectorType(AValue.Type) + " and " + GetVectorType(BValue.Type);
            AddError(Error);
            
            ShaderChunks.append("// ERROR: Type mismatch\n");
        }

        ShaderChunks.append(TypeStr + " " + OwningNode + " = min(" + AValue.Value + ", " + BValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, ResultType, EComponentMask::RGBA);
    }

    void FMaterialCompiler::Max(CMaterialInput* A, CMaterialInput* B)
    {
        CMaterialExpression_Max* Node = A->GetOwningNode<CMaterialExpression_Max>();
        
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        FInputValue AValue = GetTypedInputValue(A, Node->ConstA);
        FInputValue BValue = GetTypedInputValue(B, Node->ConstB);
        
        EMaterialValueType ResultType = DetermineResultType(AValue.Type, BValue.Type, true);
        FString TypeStr = GetVectorType(ResultType);

        // Check for invalid operations
        if (AValue.ComponentCount > 1 && BValue.ComponentCount > 1 && AValue.ComponentCount != BValue.ComponentCount)
        {
            FError Error;
            Error.ErrorNode         = A->GetOwningNode<CMaterialGraphNode>();
            Error.ErrorName         = "Type Mismatch";
            Error.ErrorDescription  = "Cannot perform Power between " + GetVectorType(AValue.Type) + " and " + GetVectorType(BValue.Type);
            AddError(Error);
            
            ShaderChunks.append("// ERROR: Type mismatch\n");
        }

        ShaderChunks.append(TypeStr + " " + OwningNode + " = max(" + AValue.Value + ", " + BValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, ResultType, EComponentMask::RGBA);
    }

    void FMaterialCompiler::Step(CMaterialInput* A, CMaterialInput* B)
    {
        CMaterialExpression_Step* Node = A->GetOwningNode<CMaterialExpression_Step>();
        
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        FInputValue AValue = GetTypedInputValue(A, Node->ConstA);
        FInputValue BValue = GetTypedInputValue(B, Node->ConstB);
        
        EMaterialValueType ResultType = DetermineResultType(AValue.Type, BValue.Type, true);
        FString TypeStr = GetVectorType(ResultType);

        // Check for invalid operations
        if (AValue.ComponentCount > 1 && BValue.ComponentCount > 1 && AValue.ComponentCount != BValue.ComponentCount)
        {
            FError Error;
            Error.ErrorNode         = A->GetOwningNode<CMaterialGraphNode>();
            Error.ErrorName         = "Type Mismatch";
            Error.ErrorDescription  = "Cannot perform Power between " + GetVectorType(AValue.Type) + " and " + GetVectorType(BValue.Type);
            AddError(Error);
            
            ShaderChunks.append("// ERROR: Type mismatch\n");
        }

        ShaderChunks.append(TypeStr + " " + OwningNode + " = step(" + AValue.Value + ", " + BValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, ResultType, EComponentMask::RGBA);
    }

    void FMaterialCompiler::Lerp(CMaterialInput* A, CMaterialInput* B, CMaterialInput* C)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        CMaterialExpression_Lerp* Node = A->GetOwningNode<CMaterialExpression_Lerp>();

        FInputValue AValue = GetTypedInputValue(A, Node->ConstA);
        FInputValue BValue = GetTypedInputValue(B, Node->ConstB);
        FInputValue CValue = GetTypedInputValue(C, Node->Alpha);
        
        // Result type is determined by A and B (should match)
        EMaterialValueType ResultType = DetermineResultType(AValue.Type, BValue.Type, true);
        FString TypeStr = GetVectorType(ResultType);

        // Check for invalid operations
        if (AValue.ComponentCount > 1 && BValue.ComponentCount > 1 && AValue.ComponentCount != BValue.ComponentCount)
        {
            FError Error;
            Error.ErrorNode         = A->GetOwningNode<CMaterialGraphNode>();
            Error.ErrorName         = "Type Mismatch";
            Error.ErrorDescription  = "Cannot perform Power between " + GetVectorType(AValue.Type) + " and " + GetVectorType(BValue.Type);
            AddError(Error);
            
            ShaderChunks.append("// ERROR: Type mismatch\n");
        }

        ShaderChunks.append(TypeStr + " " + OwningNode + " = mix(" + AValue.Value + ", " + BValue.Value + ", " + CValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, ResultType, EComponentMask::RGBA);
    }

    void FMaterialCompiler::Clamp(CMaterialInput* A, CMaterialInput* B, CMaterialInput* C)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        CMaterialExpression_Clamp* Node = A->GetOwningNode<CMaterialExpression_Clamp>();

        FInputValue XValue = GetTypedInputValue(C, "1.0f");
        FInputValue AValue = GetTypedInputValue(A, Node->ConstA);
        FInputValue BValue = GetTypedInputValue(B, Node->ConstB);
        
        EMaterialValueType ResultType = DetermineResultType(AValue.Type, BValue.Type, true);
        ResultType = DetermineResultType(ResultType, XValue.Type, true);
        FString TypeStr = GetVectorType(ResultType);

        // Check for invalid operations
        if (AValue.ComponentCount > 1 && BValue.ComponentCount > 1 && XValue.ComponentCount > 1 && AValue.ComponentCount != BValue.ComponentCount != XValue.ComponentCount)
        {
            FError Error;
            Error.ErrorNode         = A->GetOwningNode<CMaterialGraphNode>();
            Error.ErrorName         = "Type Mismatch";
            Error.ErrorDescription  = "Cannot perform clamp between " + GetVectorType(XValue.Type) + ", " + GetVectorType(AValue.Type) + " and " + GetVectorType(BValue.Type);
            AddError(Error);
            
            ShaderChunks.append("// ERROR: Type mismatch\n");
        }

        ShaderChunks.append(TypeStr + " " + OwningNode + " = clamp(" + XValue.Value + ", " + AValue.Value + ", " + BValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, ResultType, EComponentMask::RGBA);
    }

    void FMaterialCompiler::SmoothStep(CMaterialInput* A, CMaterialInput* B, CMaterialInput* C)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();
        CMaterialExpression_SmoothStep* Node = A->GetOwningNode<CMaterialExpression_SmoothStep>();

        FInputValue AValue = GetTypedInputValue(A, Node->ConstA);
        FInputValue BValue = GetTypedInputValue(B, Node->ConstB);
        FInputValue CValue = GetTypedInputValue(C, Node->X);
        
        EMaterialValueType ResultType = DetermineResultType(AValue.Type, BValue.Type, true);
        ResultType = DetermineResultType(ResultType, CValue.Type, true);
        FString TypeStr = GetVectorType(ResultType);

        // Check for invalid operations
        if (AValue.ComponentCount > 1 && BValue.ComponentCount > 1 && AValue.ComponentCount != BValue.ComponentCount)
        {
            FError Error;
            Error.ErrorNode         = A->GetOwningNode<CMaterialGraphNode>();
            Error.ErrorName         = "Type Mismatch";
            Error.ErrorDescription  = "Cannot perform smoothstep between " + GetVectorType(AValue.Type) + " and " + GetVectorType(BValue.Type);
            AddError(Error);
            
            ShaderChunks.append("// ERROR: Type mismatch\n");
        }

        ShaderChunks.append(TypeStr + " " + OwningNode + " = smoothstep(" + AValue.Value + ", " + BValue.Value + ", " + CValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, ResultType, EComponentMask::RGBA);
    }

    // ========================================================================
    // Vector Operations
    // ========================================================================

    void FMaterialCompiler::Saturate(CMaterialInput* A, CMaterialInput* /*B*/)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();

        FInputValue AValue = GetTypedInputValue(A, "0.0");
        FString TypeStr = GetVectorType(AValue.Type);

        ShaderChunks.append(TypeStr + " " + OwningNode + " = clamp(" + AValue.Value + ", " + TypeStr + "(0.0), " + TypeStr + "(1.0));\n");
        RegisterNodeOutput(OwningNode, AValue.Type, EComponentMask::RGBA);
    }

    void FMaterialCompiler::Normalize(CMaterialInput* A, CMaterialInput* /*B*/)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();

        FInputValue AValue = GetTypedInputValue(A, "vec3(0.0, 0.0, 1.0)");
        
        // Normalize requires at least vec2
        if (AValue.ComponentCount < 2)
        {
            FError Error;
            Error.ErrorName = "Invalid Type";
            Error.ErrorDescription = "Normalize requires at least a vec2 input";
            Error.ErrorNode = A->GetOwningNode<CMaterialGraphNode>();
            AddError(Error);
            
            // Default to vec3
            AValue.Value = "vec3(0.0, 0.0, 1.0)";
            AValue.Type = EMaterialValueType::Float3;
            AValue.ComponentCount = 3;
        }
        
        FString TypeStr = GetVectorType(AValue.Type);
        ShaderChunks.append(TypeStr + " " + OwningNode + " = normalize(" + AValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, AValue.Type, EComponentMask::RGBA);
    }

    void FMaterialCompiler::Distance(CMaterialInput* A, CMaterialInput* B)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();

        FInputValue AValue = GetTypedInputValue(A, "0.0");
        FInputValue BValue = GetTypedInputValue(B, "0.0");
        
        // Distance requires vectors of same dimension
        if (AValue.ComponentCount < 2)
        {
            AValue.Value = "vec3(" + AValue.Value + ")";
            AValue.ComponentCount = 3;
        }
        
        if (BValue.ComponentCount < 2)
        {
            BValue.Value = "vec3(" + BValue.Value + ")";
            BValue.ComponentCount = 3;
        }
        
        if (AValue.ComponentCount != BValue.ComponentCount)
        {
            FError Error;
            Error.ErrorName = "Type Mismatch";
            Error.ErrorDescription = "Distance requires vectors of the same dimension";
            Error.ErrorNode = A->GetOwningNode<CMaterialGraphNode>();
            AddError(Error);
        }

        ShaderChunks.append("float " + OwningNode + " = distance(" + AValue.Value + ", " + BValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, EMaterialValueType::Float, EComponentMask::R);
    }

    void FMaterialCompiler::Abs(CMaterialInput* A, CMaterialInput* /*B*/)
    {
        FString OwningNode = A->GetOwningNode()->GetNodeFullName();

        FInputValue AValue = GetTypedInputValue(A, "0.0");
        FString TypeStr = GetVectorType(AValue.Type);

        ShaderChunks.append(TypeStr + " " + OwningNode + " = abs(" + AValue.Value + ");\n");
        RegisterNodeOutput(OwningNode, AValue.Type, EComponentMask::RGBA);
    }

    void FMaterialCompiler::GetBoundTextures(TVector<TObjectHandle<CTexture>>& Images)
    {
        Images = BoundImages;
    }

    void FMaterialCompiler::AddRaw(const FString& Raw)
    {
        ShaderChunks.append(Raw);
    }
}