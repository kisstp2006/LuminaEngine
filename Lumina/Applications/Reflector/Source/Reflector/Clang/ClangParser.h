#pragma once
#include "ClangParserContext.h"

namespace Lumina::Reflection
{
    class FClangParser
    {
    public:

        FClangParser();

        bool Parse(const eastl::string& SolutionPath, eastl::vector<FReflectedHeader>& Headers, FReflectedProject& Project);
        void Dispose(FReflectedProject& Project);
        
        FClangParserContext ParsingContext;
    
    private:

    };
}
