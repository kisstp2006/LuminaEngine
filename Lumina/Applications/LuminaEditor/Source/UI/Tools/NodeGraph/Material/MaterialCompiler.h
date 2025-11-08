#pragma once
#include <sstream>

#include "MaterialInput.h"
#include "Containers/Array.h"
#include "Containers/String.h"
#include "Core/Object/ObjectHandleTyped.h"
#include "Renderer/RHIFwd.h"

namespace Lumina
{
    class CTexture;
    class FMaterialNodePin;
    class CMaterialGraphNode;
}


namespace Lumina
{
    class CMaterialGraphNode;
    class CMaterialInput;
    class CMaterialOutput;
    class CTexture;
    class FMaterialNodePin;

    enum class EMaterialValueType : uint8
    {
        Float,
        Float2,
        Float3,
        Float4
    };

    class FMaterialCompiler
    {
    public:
        struct FError
        {
            FString             ErrorName;
            FString             ErrorDescription;
            CMaterialGraphNode* ErrorNode = nullptr;
            FMaterialNodePin*   ErrorPin = nullptr;
        };

        struct FScalarParam
        {
            uint16 Index;
            float Value;
        };

        struct FVectorParam
        {
            uint16 Index;
            glm::vec4 Value;
        };

        struct FNodeOutputInfo
        {
            EMaterialValueType Type;
            EComponentMask Mask;
            FString NodeName;
        };

        struct FInputValue
        {
            FString Value;
            EMaterialValueType Type;
            int32 ComponentCount;
        };

    public:
        FMaterialCompiler();

        FString BuildTree(SIZE_T& StartReplacement, SIZE_T& EndReplacement);
        
        void ValidateConnections(CMaterialInput* A, CMaterialInput* B);

        // Parameter definitions
        void DefineFloatParameter(const FString& NodeID, const FName& ParamID, float Value);
        void DefineFloat2Parameter(const FString& NodeID, const FName& ParamID, float Value[2]);
        void DefineFloat3Parameter(const FString& NodeID, const FName& ParamID, float Value[3]);
        void DefineFloat4Parameter(const FString& NodeID, const FName& ParamID, float Value[4]);

        // Constant definitions
        void DefineConstantFloat(const FString& ID, float Value);
        void DefineConstantFloat2(const FString& ID, float Value[2]);
        void DefineConstantFloat3(const FString& ID, float Value[3]);
        void DefineConstantFloat4(const FString& ID, float Value[4]);

        // Texture operations
        void DefineTextureSample(const FString& ID);
        void TextureSample(const FString& ID, CTexture* Texture, CMaterialInput* Input);

        // Built-in inputs
        void VertexNormal(const FString& ID);
        void TexCoords(const FString& ID);
        void WorldPos(const FString& ID);
        void CameraPos(const FString& ID);
        void EntityID(const FString& ID);
        void Time(const FString& ID);

        // Math operations
        void Multiply(CMaterialInput* A, CMaterialInput* B);
        void Divide(CMaterialInput* A, CMaterialInput* B);
        void Add(CMaterialInput* A, CMaterialInput* B);
        void Subtract(CMaterialInput* A, CMaterialInput* B);
        void Sin(CMaterialInput* A, CMaterialInput* B);
        void Cos(CMaterialInput* A, CMaterialInput* B);
        void Floor(CMaterialInput* A, CMaterialInput* B);
        void Ceil(CMaterialInput* A, CMaterialInput* B);
        void Power(CMaterialInput* A, CMaterialInput* B);
        void Mod(CMaterialInput* A, CMaterialInput* B);
        void Min(CMaterialInput* A, CMaterialInput* B);
        void Max(CMaterialInput* A, CMaterialInput* B);
        void Step(CMaterialInput* A, CMaterialInput* B);
        void Lerp(CMaterialInput* A, CMaterialInput* B, CMaterialInput* C);
        void Clamp(CMaterialInput* A, CMaterialInput* B, CMaterialInput* C);
        void SmoothStep(CMaterialInput* A, CMaterialInput* B, CMaterialInput* C);

        // Vector operations
        void Saturate(CMaterialInput* A, CMaterialInput* B);
        void Normalize(CMaterialInput* A, CMaterialInput* B);
        void Distance(CMaterialInput* A, CMaterialInput* B);
        void Abs(CMaterialInput* A, CMaterialInput* B);

        void NewLine();
        void AddRaw(const FString& Raw);

        void GetBoundTextures(TVector<TObjectPtr<CTexture>>& Images);

        // Node output tracking
        void RegisterNodeOutput(const FString& NodeName, EMaterialValueType Type, EComponentMask Mask);
        FNodeOutputInfo GetNodeOutputInfo(const FString& NodeName) const;

        FORCEINLINE bool HasErrors() const { return !Errors.empty(); }
        FORCEINLINE void AddError(const FError& Error) { Errors.push_back(Error); }
        FORCEINLINE const TVector<FError>& GetErrors() const { return Errors; }

        FString GetVectorType(EMaterialValueType Type) const;
        FString GetVectorType(int32 ComponentCount) const;
        int32 GetComponentCount(EComponentMask Mask) const;
        int32 GetComponentCount(EMaterialValueType Type) const;
        EMaterialValueType GetTypeFromComponentCount(int32 Count) const;


        
        FInputValue GetTypedInputValue(CMaterialInput* Input, float DefaultValue = 0.0f);
        FInputValue GetTypedInputValue(CMaterialInput* Input, const FString& DefaultValueStr);
    private:
        
        
        EMaterialValueType DetermineResultType(EMaterialValueType A, EMaterialValueType B, bool IsComponentWise = true);
        
        void EmitBinaryOp(const FString& Op, CMaterialInput* A, CMaterialInput* B, 
                         float DefaultA, float DefaultB, bool IsComponentWise = true);

    private:
        FString ShaderChunks;
        TVector<TObjectPtr<CTexture>> BoundImages;
        TVector<FError> Errors;
        
        THashMap<FName, FScalarParam> ScalarParameters;
        THashMap<FName, FVectorParam> VectorParameters;
        THashMap<FString, FNodeOutputInfo> NodeOutputs;
        
        uint16 NumScalarParams = 0;
        uint16 NumVectorParams = 0;
        uint32 BindingIndex = 1;
    };
}
