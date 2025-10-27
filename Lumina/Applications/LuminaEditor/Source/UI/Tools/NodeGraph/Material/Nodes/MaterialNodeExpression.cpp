#include "MaterialNodeExpression.h"

#include "Core/Object/Class.h"
#include "Core/Object/Cast.h"
#include "glm/gtc/type_ptr.hpp"
#include "Renderer/RHIIncl.h"
#include "UI/Tools/NodeGraph/Material/MaterialOutput.h"
#include "UI/Tools/NodeGraph/Material/MaterialCompiler.h"


namespace Lumina
{
    
    void CMaterialExpression::BuildNode()
    {
        Output = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "Material Output", ENodePinDirection::Output, EMaterialInputType::Float));
        Output->SetShouldDrawEditor(false);
    }

    void CMaterialExpression_Math::BuildNode()
    {
        Super::BuildNode();
    }

    void CMaterialExpression_ComponentMask::BuildNode()
    {
        Super::BuildNode();

        // Input pin - accepts any vector type
        InputPin = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Input", ENodePinDirection::Input, EMaterialInputType::Float));
        
        InputPin->SetInputType(EMaterialInputType::Float4);
        InputPin->SetComponentMask(EComponentMask::RGBA);
    
        // Output pin - type depends on mask selection
        Output->SetComponentMask(GetOutputMask());
    }

    FString CMaterialExpression_ComponentMask::GetNodeDisplayName() const
    {
        FString Builder = "ComponentMask (";
        if (R)
        {
            Builder.append("R");
        }

        if (G)
        {
            Builder.append("G");
        }

        if (B)
        {
            Builder.append("B");
        }

        if (A)
        {
            Builder.append("A");
        }

        Builder.append(")");

        return Builder;
    }

    void CMaterialExpression_ComponentMask::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        FString NodeName = GetNodeFullName();
        if (!InputPin || !InputPin->HasConnection())
        {
            // No input connected - generate a default based on mask
            FString DefaultValue = GetDefaultValueForMask();
            EMaterialValueType OutputType = GetOutputTypeFromMask();
            FString TypeStr = Compiler.GetVectorType(OutputType);
            
            Compiler.AddRaw(TypeStr + " " + NodeName + " = " + DefaultValue + ";\n");
            Compiler.RegisterNodeOutput(NodeName, OutputType, GetOutputMask());
            return;
        }
        
        // Get the input value
        CMaterialOutput* ConnectedOutput = InputPin->GetConnection<CMaterialOutput>(0);
        FString InputNodeName = ConnectedOutput->GetOwningNode()->GetNodeFullName();
        
        // Get input type info
        FMaterialCompiler::FNodeOutputInfo InputInfo = Compiler.GetNodeOutputInfo(InputNodeName);
        
        // Build the swizzle mask based on R,G,B,A booleans
        FString Swizzle = BuildSwizzleMask();
        
        // Determine output type based on how many components are selected
        EMaterialValueType OutputType = GetOutputTypeFromMask();
        FString OutputTypeStr = Compiler.GetVectorType(OutputType);
        
        // Calculate actual output component count
        int32 InputComponentCount = Compiler.GetComponentCount(InputInfo.Type);
        int32 OutputComponentCount = GetSelectedComponentCount();
        
        if (OutputComponentCount > InputComponentCount)
        {
            FMaterialCompiler::FError Error;
            Error.ErrorName = "Invalid Component Mask";
            Error.ErrorDescription = "Cannot extract " + eastl::to_string(OutputComponentCount) + 
                                    " components from " + Compiler.GetVectorType(InputInfo.Type) + 
                                    " (only has " + eastl::to_string(InputComponentCount) + " components)";
            Error.ErrorNode = this;
            Compiler.AddError(Error);
            
            // Generate fallback code
            Compiler.AddRaw("// ERROR: Invalid component mask\n");
            Compiler.AddRaw(OutputTypeStr + " " + NodeName + " = " + GetDefaultValueForMask() + ";\n");
            Compiler.RegisterNodeOutput(NodeName, OutputType, GetOutputMask());
            return;
        }
        
        // If we need a swizzle to get the components
        if (!Swizzle.empty())
        {
            // Check if we need type casting
            if (OutputComponentCount == 1)
            {
                // Extracting a single component - result is float
                Compiler.AddRaw("float " + NodeName + " = " + InputNodeName + Swizzle + ";\n");
                // IMPORTANT: Register as Float1, not based on input type!
                Compiler.RegisterNodeOutput(NodeName, EMaterialValueType::Float, EComponentMask::R);
            }
            else
            {
                // Extracting multiple components - need vec constructor if not all components
                bool NeedsConstruction = (OutputComponentCount != InputComponentCount);
                
                if (NeedsConstruction)
                {
                    Compiler.AddRaw(OutputTypeStr + " " + NodeName + " = " + 
                                  OutputTypeStr + "(" + InputNodeName + Swizzle + ");\n");
                }
                else
                {
                    // Just pass through
                    Compiler.AddRaw(OutputTypeStr + " " + NodeName + " = " + InputNodeName + ";\n");
                }
                
                // Register with correct output mask based on selected components
                EComponentMask OutputMask = static_cast<EComponentMask>((1 << OutputComponentCount) - 1);
                Compiler.RegisterNodeOutput(NodeName, OutputType, OutputMask);
            }
        }
        else
        {
            // No swizzle needed - all components selected, just pass through
            Compiler.AddRaw(OutputTypeStr + " " + NodeName + " = " + InputNodeName + ";\n");
            Compiler.RegisterNodeOutput(NodeName, OutputType, GetOutputMask());
        }
    }

    ImVec2 CMaterialExpression_ComponentMask::GetMinNodeTitleBarSize() const
    {
        return ImVec2(24.0f, Super::GetMinNodeTitleBarSize().y);
    }

    FString CMaterialExpression_ComponentMask::BuildSwizzleMask() const
    {
        FString Swizzle;
    
        // Count selected components and build swizzle string
        if (R || G || B || A)
        {
            Swizzle = ".";
        
            if (R)
            {
                Swizzle += "r";
            }
            if (G)
            {
                Swizzle += "g";
            }
            if (B)
            {
                Swizzle += "b";
            }
            if (A)
            {
                Swizzle += "a";
            }
        }
    
        return Swizzle;
    }

    int32 CMaterialExpression_ComponentMask::GetSelectedComponentCount() const
    {
        int32 Count = 0;
        if (R)
        {
            Count++;
        }
        if (G)
        {
            Count++;
        }
        if (B)
        {
            Count++;
        }
        if (A)
        {
            Count++;
        }
        return Count;
    }

    EComponentMask CMaterialExpression_ComponentMask::GetOutputMask() const
    {
        uint32 Mask = 0;
    
        if (R)
        {
            Mask |= static_cast<uint32>(EComponentMask::R);
        }
        if (G)
        {
            Mask |= static_cast<uint32>(EComponentMask::G);
        }
        if (B)
        {
            Mask |= static_cast<uint32>(EComponentMask::B);
        }
        if (A)
        {
            Mask |= static_cast<uint32>(EComponentMask::A);
        }

        return static_cast<EComponentMask>(Mask);
    }

    EMaterialValueType CMaterialExpression_ComponentMask::GetOutputTypeFromMask() const
    {
        int32 Count = GetSelectedComponentCount();
    
        switch (Count)
        {
        case 1: return EMaterialValueType::Float;
        case 2: return EMaterialValueType::Float2;
        case 3: return EMaterialValueType::Float3;
        case 4: return EMaterialValueType::Float4;
        default: return EMaterialValueType::Float;
        }
    }

    FString CMaterialExpression_ComponentMask::GetDefaultValueForMask() const
    {
        int32 Count = GetSelectedComponentCount();
    
        switch (Count)
        {
        case 1: return "0.0";
        case 2: return "vec2(0.0)";
        case 3: return "vec3(0.0)";
        case 4: return "vec4(0.0)";
        default: return "0.0";
        }
    }

    void CMaterialExpression_Append::BuildNode()
    {
        CMaterialExpression::BuildNode();

        // Input A - first component(s)
        InputA = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "A", ENodePinDirection::Input, EMaterialInputType::Float4));
        InputA->SetInputType(EMaterialInputType::Float4);
        InputA->SetComponentMask(EComponentMask::RGBA);
    
        // Input B - second component(s)
        InputB = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "B", ENodePinDirection::Input, EMaterialInputType::Float4));
        InputB->SetInputType(EMaterialInputType::Float4);
        InputB->SetComponentMask(EComponentMask::RGBA);
    
        // Output pin - combined components
        Output->SetComponentMask(EComponentMask::RGBA);
    }

    void CMaterialExpression_Append::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        FString NodeName = GetNodeFullName();
        
        // Get input values with proper types
        FMaterialCompiler::FInputValue AValue = Compiler.GetTypedInputValue(InputA, 0.0f);
        FMaterialCompiler::FInputValue BValue = Compiler.GetTypedInputValue(InputB, 0.0f);
        
        // Calculate total component count
        int32 TotalComponents = AValue.ComponentCount + BValue.ComponentCount;
        
        // Validate we don't exceed vec4
        if (TotalComponents > 4)
        {
            FMaterialCompiler::FError Error;
            Error.ErrorName = "Too Many Components";
            Error.ErrorDescription = "Cannot append " + 
                                    Compiler.GetVectorType(AValue.Type) + " and " + 
                                    Compiler.GetVectorType(BValue.Type) + 
                                    " (total " + eastl::to_string(TotalComponents) + 
                                    " components exceeds vec4 limit)";
            Error.ErrorNode = this;
            Compiler.AddError(Error); 
            
            // Generate error fallback
            Compiler.AddRaw("// ERROR: Too many components to append\n");
            Compiler.AddRaw("vec4 " + NodeName + " = vec4(0.0);\n");
            Compiler.RegisterNodeOutput(NodeName, EMaterialValueType::Float4, EComponentMask::RGBA);
            return;
        }
        
        // Determine output type
        EMaterialValueType OutputType = Compiler.GetTypeFromComponentCount(TotalComponents);
        FString OutputTypeStr = Compiler.GetVectorType(OutputType);
        
        // Generate the append operation
        if (AValue.ComponentCount == 1 && BValue.ComponentCount == 1)
        {
            // float + float = vec2
            Compiler.AddRaw("vec2 " + NodeName + " = vec2(" + AValue.Value + ", " + BValue.Value + ");\n");
        }
        else if (AValue.ComponentCount == 1)
        {
            // float + vecN = vecN+1
            Compiler.AddRaw(OutputTypeStr + " " + NodeName + " = " + OutputTypeStr + "(" + AValue.Value + ", " + BValue.Value + ");\n");
        }
        else if (BValue.ComponentCount == 1)
        {
            // vecN + float = vecN+1
            Compiler.AddRaw(OutputTypeStr + " " + NodeName + " = " + OutputTypeStr + "(" + AValue.Value + ", " + BValue.Value + ");\n");
        }
        else
        {
            // vecN + vecM = vecN+M
            Compiler.AddRaw(OutputTypeStr + " " + NodeName + " = " + OutputTypeStr + "(" + AValue.Value + ", " + BValue.Value + ");\n");
        }
        
        // Register output
        EComponentMask OutputMask = static_cast<EComponentMask>((1 << TotalComponents) - 1);
        Compiler.RegisterNodeOutput(NodeName, OutputType, OutputMask);
    }

    void CMaterialExpression_WorldPos::BuildNode()
    {
        CMaterialExpression::BuildNode();
    }

    void CMaterialExpression_WorldPos::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.WorldPos(FullName);
    }

    void CMaterialExpression_CameraPos::BuildNode()
    {
        CMaterialExpression::BuildNode();
    }

    void CMaterialExpression_CameraPos::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.CameraPos(FullName);
    }

    void CMaterialExpression_EntityID::BuildNode()
    {
        CMaterialExpression::BuildNode();
    }

    void CMaterialExpression_EntityID::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.EntityID(FullName);
    }

    void CMaterialExpression_VertexNormal::BuildNode()
    {
        CMaterialExpression::BuildNode();
    }

    void CMaterialExpression_VertexNormal::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.VertexNormal(FullName);
    }

    void CMaterialExpression_TexCoords::BuildNode()
    {
        CMaterialExpression::BuildNode();
    }

    void CMaterialExpression_TexCoords::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.TexCoords(FullName);
    }

    void CMaterialExpression_Constant::DrawContextMenu()
    {
        const char* MenuItem = bDynamic ? "Make Constant" : "Make Parameter";
        if (ImGui::MenuItem(MenuItem))
        {
            bDynamic = !bDynamic;
            if (bDynamic)
            {
                ParameterName = GetNodeDisplayName() + "_Param";
            }
        }
    }

    void CMaterialExpression_Constant::DrawNodeTitleBar()
    {
        if (bDynamic)
        {
            ImGui::SetNextItemWidth(125);
            if (ImGui::InputText("##", const_cast<char*>(ParameterName.c_str()), 256))
            {
                
            }
        }
        else
        {
            CMaterialExpression::DrawNodeTitleBar();
        }
    }

    void CMaterialExpression_Constant::BuildNode()
    {
        switch (ValueType)
        {
        case EMaterialInputType::Float:
            {
                CMaterialOutput* ValuePin = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "X", ENodePinDirection::Output, EMaterialInputType::Float));
                ValuePin->SetShouldDrawEditor(true);
                ValuePin->SetHideDuringConnection(false);
                ValuePin->SetPinName("X");
                ValuePin->InputType = EMaterialInputType::Float;
            }
            break;
        case EMaterialInputType::Float2:
            {
                CMaterialOutput* ValuePin = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "XY", ENodePinDirection::Output, EMaterialInputType::Float2));
                ValuePin->SetShouldDrawEditor(true);
                ValuePin->SetHideDuringConnection(false);
                ValuePin->SetPinName("XY");
                ValuePin->InputType = EMaterialInputType::Float2;
                
                CMaterialOutput* R = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "X", ENodePinDirection::Output, EMaterialInputType::Float));
                R->SetPinColor(IM_COL32(255, 10, 10, 255));
                R->SetHideDuringConnection(false);
                R->SetPinName("X");
                R->SetComponentMask(EComponentMask::R);

                CMaterialOutput* G = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "Y", ENodePinDirection::Output, EMaterialInputType::Float));
                G->SetPinColor(IM_COL32(10, 255, 10, 255));
                G->SetHideDuringConnection(false);
                G->SetPinName("Y");
                G->SetComponentMask(EComponentMask::G);

            }
            break;
        case EMaterialInputType::Float3:
            {
                CMaterialOutput* ValuePin = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "RGB", ENodePinDirection::Output, EMaterialInputType::Float3));
                ValuePin->SetShouldDrawEditor(true);
                ValuePin->SetHideDuringConnection(false);
                ValuePin->SetPinName("RGB");
                ValuePin->InputType = EMaterialInputType::Float3;
                
                CMaterialOutput* R = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "R", ENodePinDirection::Output, EMaterialInputType::Float));
                R->SetPinColor(IM_COL32(255, 10, 10, 255));
                R->SetHideDuringConnection(false);
                R->SetPinName("R");
                R->SetComponentMask(EComponentMask::R);

                CMaterialOutput* G = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "G", ENodePinDirection::Output, EMaterialInputType::Float));
                G->SetPinColor(IM_COL32(10, 255, 10, 255));
                G->SetHideDuringConnection(false);
                G->SetPinName("G");
                G->SetComponentMask(EComponentMask::G);

                CMaterialOutput* B = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "B", ENodePinDirection::Output, EMaterialInputType::Float2));
                B->SetPinColor(IM_COL32(10, 10, 255, 255));
                B->SetHideDuringConnection(false);
                B->SetPinName("B");
                B->SetComponentMask(EComponentMask::B);

            }
            break;
        case EMaterialInputType::Float4:
            {
                CMaterialOutput* ValuePin = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "RGBA", ENodePinDirection::Output, EMaterialInputType::Float4));
                ValuePin->SetShouldDrawEditor(true);
                ValuePin->SetHideDuringConnection(false);
                ValuePin->SetPinName("RGBA");
                ValuePin->InputType = EMaterialInputType::Float4;

                
                CMaterialOutput* R = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "R", ENodePinDirection::Output, EMaterialInputType::Float));
                R->SetPinColor(IM_COL32(255, 10, 10, 255));
                R->SetHideDuringConnection(false);
                R->SetPinName("R");
                R->SetComponentMask(EComponentMask::R);

                CMaterialOutput* G = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "G", ENodePinDirection::Output, EMaterialInputType::Float));
                G->SetPinColor(IM_COL32(10, 255, 10, 255));
                G->SetHideDuringConnection(false);
                G->SetPinName("G");
                G->SetComponentMask(EComponentMask::G);

                CMaterialOutput* B = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "B", ENodePinDirection::Output, EMaterialInputType::Float));
                B->SetPinColor(IM_COL32(10, 10, 255, 255));
                B->SetHideDuringConnection(false);
                B->SetPinName("B");
                B->SetComponentMask(EComponentMask::B);

                CMaterialOutput* A = Cast<CMaterialOutput>(CreatePin(CMaterialOutput::StaticClass(), "A", ENodePinDirection::Output, EMaterialInputType::Float));
                A->SetHideDuringConnection(false);
                A->SetPinName("A");
                A->SetComponentMask(EComponentMask::A);

            }
            break;
        case EMaterialInputType::Wildcard:
            break;
        }
    }

    uint32 CMaterialExpression_ConstantFloat::GenerateExpression(FMaterialCompiler& Compiler)
    {
        return 0;
    }

    void CMaterialExpression_ConstantFloat::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        if (bDynamic)
        {
            Compiler.DefineFloatParameter(FullName, ParameterName, Value.r);
        }
        else
        {
            Compiler.DefineConstantFloat(FullName, Value.r);
        }
    }

    void CMaterialExpression_ConstantFloat::DrawNodeBody()
    {
        ImGui::SetNextItemWidth(126.0f);
        ImGui::DragFloat("##", glm::value_ptr(Value), 0.01f);
    }

    uint32 CMaterialExpression_ConstantFloat2::GenerateExpression(FMaterialCompiler& Compiler)
    {
        return 0;
    }

    void CMaterialExpression_ConstantFloat2::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        if (bDynamic)
        {
            Compiler.DefineFloat2Parameter(FullName, ParameterName, &Value.r);
        }
        else
        {
            Compiler.DefineConstantFloat2(FullName, &Value.r);
        }
    }

    void CMaterialExpression_ConstantFloat2::DrawNodeBody()
    {
        ImGui::SetNextItemWidth(126.0f);
        ImGui::DragFloat2("##", glm::value_ptr(Value), 0.01f);
    }


    uint32 CMaterialExpression_ConstantFloat3::GenerateExpression(FMaterialCompiler& Compiler)
    {
        return 0;
    }

    void CMaterialExpression_ConstantFloat3::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        if (bDynamic)
        {
            Compiler.DefineFloat3Parameter(FullName, ParameterName, &Value.r);
        }
        else
        {
            Compiler.DefineConstantFloat3(FullName, &Value.r);
        }
    }

    void CMaterialExpression_ConstantFloat3::DrawNodeBody()
    {
        ImGui::SetNextItemWidth(126.0f);
        ImGui::ColorPicker3("##", glm::value_ptr(Value));
    }


    uint32 CMaterialExpression_ConstantFloat4::GenerateExpression(FMaterialCompiler& Compiler)
    {
        return 0;
    }
    
    void CMaterialExpression_ConstantFloat4::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        if (bDynamic)
        {
            Compiler.DefineFloat4Parameter(FullName, ParameterName, &Value.r);
        }
        else
        {
            Compiler.DefineConstantFloat4(FullName, &Value.r);
        }
    }

    void CMaterialExpression_ConstantFloat4::DrawNodeBody()
    {
        ImGui::SetNextItemWidth(126.0f);
        ImGui::ColorPicker4("##", glm::value_ptr(Value));
    }


    void CMaterialExpression_Addition::BuildNode()
    {
        CMaterialExpression_Math::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Y", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Y");
        B->SetShouldDrawEditor(true);
        B->SetIndex(1);

    }

    void CMaterialExpression_Addition::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Add(A, B);
    }

    void CMaterialExpression_Clamp::BuildNode()
    {
        CMaterialExpression_Math::BuildNode();

        X = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        X->SetPinName("X");
        X->SetShouldDrawEditor(true);
        X->SetIndex(0);
        
        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "A", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("A");
        A->SetShouldDrawEditor(true);
        A->SetIndex(1);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "B", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("B");
        B->SetShouldDrawEditor(true);
        B->SetIndex(2);
    }

    void CMaterialExpression_Clamp::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Clamp(A, B, X);
    }

    void CMaterialExpression_Saturate::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
    }

    void CMaterialExpression_Saturate::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Saturate(A, B);
    }

    void CMaterialExpression_Normalize::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
    }

    void CMaterialExpression_Normalize::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Normalize(A, B);
    }

    void CMaterialExpression_Distance::BuildNode()
    {
        Super::BuildNode();
        
        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Y", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Y");
        B->SetShouldDrawEditor(true);
        B->SetIndex(1);
    }

    void CMaterialExpression_Distance::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Distance(A, B);
    }

    void CMaterialExpression_Abs::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
    }

    void CMaterialExpression_Abs::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Abs(A, B);
    }

    void CMaterialExpression_SmoothStep::BuildNode()
    {
        CMaterialExpression_Math::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Edge0", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("Edge0");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Edge1", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Edge1");
        B->SetShouldDrawEditor(true);
        B->SetIndex(1);

        C = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        C->SetPinName("X");
        C->SetShouldDrawEditor(true);
        C->SetIndex(2);
    }

    void CMaterialExpression_SmoothStep::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.SmoothStep(A, B, C);
    }

    void CMaterialExpression_Subtraction::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Y", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Y");
        B->SetShouldDrawEditor(true);
        B->SetIndex(1);
    }


    void CMaterialExpression_Subtraction::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Subtract(A, B);
    }

    void CMaterialExpression_Multiplication::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Y", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Y");
        B->SetShouldDrawEditor(true);
        B->SetIndex(1);
    }
    
    void CMaterialExpression_Multiplication::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Multiply(A, B);
    }

    void CMaterialExpression_Sin::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
    }

    void CMaterialExpression_Sin::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Sin(A, B);
    }

    void CMaterialExpression_Cosin::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
    }

    void CMaterialExpression_Cosin::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Cos(A, B);
    }

    void CMaterialExpression_Floor::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
    }

    void CMaterialExpression_Floor::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Floor(A, B);
    }

    void CMaterialExpression_Ceil::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        
    }

    void CMaterialExpression_Ceil::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Ceil(A, B);
    }

    void CMaterialExpression_Power::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Y", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Y");
        B->SetShouldDrawEditor(true);
        B->SetIndex(3);
        
    }

    void CMaterialExpression_Power::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Power(A, B);
    }

    void CMaterialExpression_Mod::BuildNode()
    {
        Super::BuildNode();
        
        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Y", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Y");
        B->SetShouldDrawEditor(true);
        B->SetIndex(1);
    }

    void CMaterialExpression_Min::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Y", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Y");
        B->SetShouldDrawEditor(true);
        B->SetIndex(1);
    }

    void CMaterialExpression_Max::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Y", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Y");
        B->SetShouldDrawEditor(true);
        B->SetIndex(1);
    }

    void CMaterialExpression_Step::BuildNode()
    {
        Super::BuildNode();
        
        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);

        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Y", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Y");
        B->SetShouldDrawEditor(true);
        B->SetIndex(1);

    }

    void CMaterialExpression_Step::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Step(A, B);
    }

    void CMaterialExpression_Lerp::BuildNode()
    {
        Super::BuildNode();

        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Y", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Y");
        B->SetShouldDrawEditor(true);
        B->SetIndex(1);

        C = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "A", ENodePinDirection::Input, EMaterialInputType::Float));
        C->SetPinName("A");
        C->SetShouldDrawEditor(true);
        C->SetIndex(2);
    }

    void CMaterialExpression_Lerp::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Lerp(A, B, C);
    }

    void CMaterialExpression_Max::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Max(A, B);
    }

    void CMaterialExpression_Min::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Min(A, B);
    }

    void CMaterialExpression_Mod::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Mod(A, B);
    }

    void CMaterialExpression_Division::BuildNode()
    {
        Super::BuildNode();
        
        A = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "X", ENodePinDirection::Input, EMaterialInputType::Float));
        A->SetPinName("X");
        A->SetShouldDrawEditor(true);
        A->SetIndex(0);
        
        B = Cast<CMaterialInput>(CreatePin(CMaterialInput::StaticClass(), "Y", ENodePinDirection::Input, EMaterialInputType::Float));
        B->SetPinName("Y");
        B->SetShouldDrawEditor(true);
        B->SetIndex(1);
    }

    void CMaterialExpression_Division::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        Compiler.Divide(A, B);
    }
}
