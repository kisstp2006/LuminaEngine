#pragma once
#include "MaterialGraphNode.h"
#include "UI/Tools/NodeGraph/Material/MaterialInput.h"
#include "UI/Tools/NodeGraph/Material/MaterialOutput.h"
#include "UI/Tools/NodeGraph/Material/MaterialCompiler.h"
#include "MaterialNodeExpression.generated.h"

namespace Lumina
{
    LUM_CLASS()
    class CMaterialExpression : public CMaterialGraphNode
    {
        GENERATED_BODY()
        
    public:
        
        void BuildNode() override;
        
        CMaterialOutput* Output;

        LUM_PROPERTY(Editable, Category = "Dynamic")
        bool bDynamic = false;
        
    };

    LUM_CLASS()
    class CMaterialExpression_Math : public CMaterialExpression
    {
        GENERATED_BODY()
        
    public:
        void BuildNode() override;
        
        FInlineString GetNodeCategory() const override { return "Math"; }
        
        CMaterialInput* A = nullptr;
        CMaterialInput* B = nullptr;

        LUM_PROPERTY(Editable, Category = "Value")
        float ConstA = 0;

        LUM_PROPERTY(Editable, Category = "Value")
        float ConstB = 0;
    };

    LUM_CLASS()
    class CMaterialExpression_Addition : public CMaterialExpression_Math
    {
        GENERATED_BODY()
        
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Add"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
        
    };

    LUM_CLASS()
    class CMaterialExpression_Clamp : public CMaterialExpression_Math
    {
        GENERATED_BODY()
        
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Clamp"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

        CMaterialInput* X = nullptr;
        
    };

    LUM_CLASS()
    class CMaterialExpression_Saturate : public CMaterialExpression_Math
    {
        GENERATED_BODY()
        
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Saturate"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
        
    };
    
    LUM_CLASS()
    class CMaterialExpression_Normalize : public CMaterialExpression_Math
    {
        GENERATED_BODY()
        
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Normalize"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
        
    };

    LUM_CLASS()
    class CMaterialExpression_Distance : public CMaterialExpression_Math
    {
        GENERATED_BODY()
        
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Distance"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
        
    };

    LUM_CLASS()
    class CMaterialExpression_Abs : public CMaterialExpression_Math
    {
        GENERATED_BODY()
        
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Abs"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
        
    };
    
    LUM_CLASS()
    class CMaterialExpression_SmoothStep : public CMaterialExpression_Math
    {
        GENERATED_BODY()
        
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "SmoothStep"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

        CMaterialInput* C = nullptr;

        
        LUM_PROPERTY(Editable, Category = "Value")
        float X = 0.5f;
        
    };
    
    LUM_CLASS()
    class CMaterialExpression_Subtraction : public CMaterialExpression_Math
    {
        GENERATED_BODY()
        
    public:
        
        void BuildNode() override;


        FString GetNodeDisplayName() const override { return "Subtract"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
    };

    LUM_CLASS()
    class CMaterialExpression_Multiplication : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Multiply"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
    };
    

    LUM_CLASS()
    class CMaterialExpression_Sin : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Sin"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

    };

    LUM_CLASS()
    class CMaterialExpression_Cosin : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Cosin"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

    };

    LUM_CLASS()
    class CMaterialExpression_Floor : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Floor"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

    };

    LUM_CLASS()
    class CMaterialExpression_Ceil : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Ceil"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

    };

    LUM_CLASS()
    class CMaterialExpression_Power : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Power"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

    };

    LUM_CLASS()
    class CMaterialExpression_Mod : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Mod"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

    };

    LUM_CLASS()
    class CMaterialExpression_Min : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Min"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

    };

    LUM_CLASS()
    class CMaterialExpression_Max : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Max"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

    };

    LUM_CLASS()
    class CMaterialExpression_Step : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Step"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
        
    };
    
    LUM_CLASS()
    class CMaterialExpression_Lerp : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Lerp"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

        LUM_PROPERTY(Editable, Category = "Value")
        float Alpha = 0;

        
        CMaterialInput* C = nullptr;
    };
    
    
    LUM_CLASS()
    class CMaterialExpression_Division : public CMaterialExpression_Math
    {
        GENERATED_BODY()
    public:
        
        void BuildNode() override;

        FString GetNodeDisplayName() const override { return "Divide"; }
        void* GetNodeDefaultValue() override { return &ConstA; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
        
    };


    //============================================================================================

    LUM_CLASS()
    class CMaterialExpression_ComponentMask : public CMaterialExpression
    {
        GENERATED_BODY()
    public:
        
        void BuildNode() override;

        FInlineString GetNodeCategory() const override { return "Utility"; }
        FString GetNodeDisplayName() const override { return "ComponentMask"; }
        void* GetNodeDefaultValue() override { return nullptr; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

        FString BuildSwizzleMask() const;
        int32 GetSelectedComponentCount() const;
        EComponentMask GetOutputMask() const;
        EMaterialValueType GetOutputTypeFromMask() const;
        FString GetDefaultValueForMask() const;

        CMaterialInput* InputPin = nullptr;
        
        LUM_PROPERTY(Editable)
        bool R = true;

        LUM_PROPERTY(Editable)
        bool G = true;

        LUM_PROPERTY(Editable)
        bool B = true;

        LUM_PROPERTY(Editable)
        bool A = true;
        
    };

    LUM_CLASS()
    class CMaterialExpression_Append : public CMaterialExpression
    {
        GENERATED_BODY()
    public:
        
        void BuildNode() override;

        FInlineString GetNodeCategory() const override { return "Utility"; }
        FString GetNodeDisplayName() const override { return "Append"; }
        void* GetNodeDefaultValue() override { return nullptr; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

        
        CMaterialInput* InputA = nullptr;
        CMaterialInput* InputB = nullptr;        
    };

    LUM_CLASS()
    class CMaterialExpression_WorldPos : public CMaterialExpression
    {
        GENERATED_BODY()
    public:
        
        void BuildNode() override;

        FInlineString GetNodeCategory() const override { return "Utility"; }
        FString GetNodeDisplayName() const override { return "WorldPosition"; }
        void* GetNodeDefaultValue() override { return nullptr; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
    };

    LUM_CLASS()
    class CMaterialExpression_CameraPos : public CMaterialExpression
    {
        GENERATED_BODY()
    public:
        
        void BuildNode() override;

        FInlineString GetNodeCategory() const override { return "Utility"; }
        FString GetNodeDisplayName() const override { return "CameraPosition"; }
        void* GetNodeDefaultValue() override { return nullptr; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
    };

    LUM_CLASS()
    class CMaterialExpression_EntityID : public CMaterialExpression
    {
        GENERATED_BODY()
    public:
        
        void BuildNode() override;

        FInlineString GetNodeCategory() const override { return "Utility"; }
        FString GetNodeDisplayName() const override { return "EntityID"; }
        void* GetNodeDefaultValue() override { return nullptr; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
    };

    LUM_CLASS()
    class CMaterialExpression_VertexNormal : public CMaterialExpression
    {
        GENERATED_BODY()
    public:
        
        void BuildNode() override;

        FInlineString GetNodeCategory() const override { return "Utility"; }
        FString GetNodeDisplayName() const override { return "VertexNormal"; }
        void* GetNodeDefaultValue() override { return nullptr; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
    };

    LUM_CLASS()
    class CMaterialExpression_TexCoords : public CMaterialExpression
    {
        GENERATED_BODY()
    public:
        
        void BuildNode() override;

        FInlineString GetNodeCategory() const override { return "Utility"; }
        FString GetNodeDisplayName() const override { return "TexCoords"; }
        void* GetNodeDefaultValue() override { return nullptr; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override { return 0; }
        void GenerateDefinition(FMaterialCompiler& Compiler) override;
    };

    //============================================================================================


    LUM_CLASS()
    class CMaterialExpression_Constant : public CMaterialExpression
    {
        GENERATED_BODY()
    public:
        
        
        FInlineString GetNodeCategory() const override { return "Constants"; }
        void DrawContextMenu() override;
        void DrawNodeTitleBar() override;
        void BuildNode() override;
        
        void* GetNodeDefaultValue() override { return &Value.r; }

        LUM_PROPERTY(Editable, Category = "Parameter")
        FName               ParameterName;

        LUM_PROPERTY(Editable, Color, Category = "Value")
        glm::vec4           Value = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        
        EMaterialInputType  ValueType = EMaterialInputType::Wildcard;
    };

    LUM_CLASS()
    class CMaterialExpression_ConstantFloat : public CMaterialExpression_Constant
    {
        GENERATED_BODY()
    public:
        
        CMaterialExpression_ConstantFloat()
        {
            ValueType = EMaterialInputType::Float;
        }

        FString GetNodeDisplayName() const override { return "Float"; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override;
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

        void DrawNodeBody() override;
        
    };

    LUM_CLASS()
    class CMaterialExpression_ConstantFloat2 : public CMaterialExpression_Constant
    {
        GENERATED_BODY()
    public:

        CMaterialExpression_ConstantFloat2()
        {
            ValueType = EMaterialInputType::Float2;
        }
        
        FString GetNodeDisplayName() const override { return "Vec2"; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override;
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

        void DrawNodeBody() override;
    };

    LUM_CLASS()
    class CMaterialExpression_ConstantFloat3 : public CMaterialExpression_Constant
    {
        GENERATED_BODY()
    public:
        
        
        CMaterialExpression_ConstantFloat3()
        {
            ValueType = EMaterialInputType::Float3;
        }

        FString GetNodeDisplayName() const override { return "Vec3"; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override;
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

        void DrawNodeBody() override;
        
    };

    LUM_CLASS()
    class CMaterialExpression_ConstantFloat4 : public CMaterialExpression_Constant
    {
        GENERATED_BODY()
    public:
        
        CMaterialExpression_ConstantFloat4()
        {
            ValueType = EMaterialInputType::Float4;
        }

        FString GetNodeDisplayName() const override { return "Vec4"; }
        uint32 GenerateExpression(FMaterialCompiler& Compiler) override;
        void GenerateDefinition(FMaterialCompiler& Compiler) override;

        void DrawNodeBody() override;

    };
}
