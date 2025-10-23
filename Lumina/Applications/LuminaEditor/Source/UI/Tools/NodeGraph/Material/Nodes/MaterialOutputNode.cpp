#include "MaterialOutputNode.h"

#include "Renderer/RHIIncl.h"
#include "UI/Tools/NodeGraph/Material/MaterialInput.h"
#include "UI/Tools/NodeGraph/Material/MaterialCompiler.h"
#include "UI/Tools/NodeGraph/Material/MaterialOutput.h"

namespace Lumina
{
    FString CMaterialOutputNode::GetNodeDisplayName() const
    {
        return "Material Output";
    }
    
    FString CMaterialOutputNode::GetNodeTooltip() const
    {
        return "The final output to the shader";
    }


    void CMaterialOutputNode::BuildNode()
    {
        // Base Color (Albedo)
        BaseColorPin = CreatePin(CMaterialInput::StaticClass(), "Base Color (RGBA)", ENodePinDirection::Input, EMaterialInputType::Float3);
        BaseColorPin->SetPinName("Base Color (RGBA)");
    
        // Metallic (Determines if the material is metal or non-metal)
        MetallicPin = CreatePin(CMaterialInput::StaticClass(), "Metallic", ENodePinDirection::Input, EMaterialInputType::Float);
        MetallicPin->SetPinName("Metallic");
        
        // Roughness (Controls how smooth or rough the surface is)
        RoughnessPin = CreatePin(CMaterialInput::StaticClass(), "Roughness", ENodePinDirection::Input, EMaterialInputType::Float);
        RoughnessPin->SetPinName("Roughness");

        // Specular (Affects intensity of reflections for non-metals)
        SpecularPin = CreatePin(CMaterialInput::StaticClass(), "Specular", ENodePinDirection::Input, EMaterialInputType::Float);
        SpecularPin->SetPinName("Specular");

        // Emissive (Self-illumination, for glowing objects)
        EmissivePin = CreatePin(CMaterialInput::StaticClass(), "Emissive", ENodePinDirection::Input, EMaterialInputType::Float3);
        EmissivePin->SetPinName("Emissive (RGB)");

        // Ambient Occlusion (Shadows in crevices to add realism)
        AOPin = CreatePin(CMaterialInput::StaticClass(), "Ambient Occlusion", ENodePinDirection::Input, EMaterialInputType::Float);
        AOPin->SetPinName("Ambient Occlusion");

        // Normal Map (For surface detail)
        NormalPin = CreatePin(CMaterialInput::StaticClass(), "Normal Map (XYZ)", ENodePinDirection::Input, EMaterialInputType::Float3);
        NormalPin->SetPinName("Normal Map (XYZ)");
        
        // Opacity (For transparent materials)
        OpacityPin = CreatePin(CMaterialInput::StaticClass(), "Opacity", ENodePinDirection::Input, EMaterialInputType::Float);
        OpacityPin->SetPinName("Opacity");

        // Opacity (For transparent materials)
        WorldPositionOffsetPin = CreatePin(CMaterialInput::StaticClass(), "World Position Offset (WPO)", ENodePinDirection::Input, EMaterialInputType::Float3);
        WorldPositionOffsetPin->SetPinName("World Position Offset (WPO)");
    }

    void CMaterialOutputNode::GenerateDefinition(FMaterialCompiler& Compiler)
    {
        FString Output;
        Output += "\n\n";
    
        Output += "SMaterialInputs GetMaterialInputs()\n{\n";
        Output += "\tSMaterialInputs Input;\n";
    
        auto EmitMaterialInput = [&](const FString& InputName, CEdNodeGraphPin* Pin, 
                                      const FString& DefaultValue, int32 RequiredComponents)
        {
            Output += "\tInput." + InputName + " = ";
            
            if (Pin->HasConnection())
            {
                CMaterialOutput* ConnectedPin = Pin->GetConnection<CMaterialOutput>(0);
                FString NodeName = ConnectedPin->GetOwningNode()->GetNodeFullName();
                
                // Get the type info for the connected node
                FMaterialCompiler::FNodeOutputInfo Info = Compiler.GetNodeOutputInfo(NodeName);
                int32 ConnectedComponents = Compiler.GetComponentCount(Info.Type);
                
                // Check if there's a component mask applied
                FString Swizzle = GetSwizzleForMask(ConnectedPin->GetComponentMask());
                
                // If swizzle is applied, count its components
                if (!Swizzle.empty())
                {
                    ConnectedComponents = Swizzle.length() - 1; // -1 for the '.' character
                }
                
                // Build the value string
                FString Value = NodeName + Swizzle;
                
                // Handle type conversion based on what we have vs what we need
                if (RequiredComponents == 1)
                {
                    // Need a scalar (float)
                    if (ConnectedComponents == 1)
                    {
                        // float -> float (perfect match)
                        Output += Value + ";\n";
                    }
                    else
                    {
                        // vecN -> float (extract first component)
                        if (Swizzle.empty())
                        {
                            Output += Value + ".r;\n";
                        }
                        else
                        {
                            // Swizzle already applied, just take first component
                            Output += Value + ".r;\n";
                        }
                    }
                }
                else if (RequiredComponents == 3)
                {
                    // Need a vec3
                    if (ConnectedComponents == 3)
                    {
                        // vec3 -> vec3 (perfect match)
                        Output += Value + ";\n";
                    }
                    else if (ConnectedComponents == 4)
                    {
                        // vec4 -> vec3 (extract RGB)
                        if (Swizzle.empty())
                        {
                            Output += Value + ".rgb;\n";
                        }
                        else
                        {
                            Output += Value + ";\n";
                        }
                    }
                    else if (ConnectedComponents == 2)
                    {
                        // vec2 -> vec3 (pad with 0)
                        Output += "vec3(" + Value + ", 0.0);\n";
                    }
                    else // ConnectedComponents == 1
                    {
                        // float -> vec3 (broadcast)
                        Output += "vec3(" + Value + ");\n";
                    }
                }
                else
                {
                    // Generic case - use the value as-is
                    Output += Value + ";\n";
                }
            }
            else
            {
                Output += DefaultValue + ";\n";
            }
        };
    
        // Emit all material inputs with their required component counts
        EmitMaterialInput("Diffuse", BaseColorPin, "vec3(1.0, 1.0, 1.0)", 3);
        EmitMaterialInput("Metallic", MetallicPin, "0.0", 1);
        EmitMaterialInput("Roughness", RoughnessPin, "1.0", 1);
        EmitMaterialInput("Specular", SpecularPin, "0.5", 1);
        EmitMaterialInput("Emissive", EmissivePin, "vec3(0.0, 0.0, 0.0)", 3);
        EmitMaterialInput("AmbientOcclusion", AOPin, "1.0", 1);
        EmitMaterialInput("Normal", NormalPin, "vec3(0.0, 0.0, 1.0)", 3);
        EmitMaterialInput("Opacity", OpacityPin, "1.0", 1);
        EmitMaterialInput("WorldPositionOffset", WorldPositionOffsetPin, "vec3(0.0)", 3);
    
        Output += "\treturn Input;\n}\n";
    
        Compiler.AddRaw(Output);
    }
}
