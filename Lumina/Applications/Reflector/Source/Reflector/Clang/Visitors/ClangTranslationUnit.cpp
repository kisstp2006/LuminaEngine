#include "ClangTranslationUnit.h"
#include "ClangVisitor.h"
#include "Reflector/Clang/ClangParserContext.h"
#include "Reflector/Clang/Utils.h"

namespace Lumina::Reflection
{
    uint64_t GTranslationUnitsVisited;
    uint64_t GTranslationUnitsParsed;
    
    CXChildVisitResult VisitTranslationUnit(CXCursor Cursor, CXCursor Parent, CXClientData ClientData)
    {
        GTranslationUnitsVisited++;
        
        CXSourceLocation Loc = clang_getCursorLocation(Cursor);
        if (clang_Location_isInSystemHeader(Loc))
        {
            return CXChildVisit_Continue;
        }
        
        FClangParserContext* ParserContext = (FClangParserContext*)ClientData;
        
        eastl::string FilePath = ClangUtils::GetHeaderPathForCursor(Cursor);
        FStringHash Hash = FStringHash(FilePath);

        auto Iter = ParserContext->Project->HeaderHashMap.find(Hash);
        if (Iter == ParserContext->Project->HeaderHashMap.end())
        {
            return CXChildVisit_Continue;
        }
        
        GTranslationUnitsParsed++;
        
        ParserContext->ReflectedHeader = &Iter->second;
        
        CXCursorKind CursorKind = clang_getCursorKind(Cursor);
        eastl::string CursorName = ClangUtils::GetCursorDisplayName(Cursor);
        
        switch (CursorKind)  // NOLINT(clang-diagnostic-switch-enum)
        {
            case (CXCursor_MacroExpansion):
                {
                    return Visitor::VisitMacro(Cursor, Parent, ParserContext);
                }
            
            case(CXCursor_ClassDecl):
                {
                    ParserContext->PushNamespace(CursorName);
                    clang_visitChildren(Cursor, VisitTranslationUnit, ClientData);
                    ParserContext->PopNamespace();
                    
                    return Visitor::VisitClass(Cursor, Parent, ParserContext);
                }
            
            case(CXCursor_StructDecl):
                {
                    ParserContext->PushNamespace(CursorName);
                    clang_visitChildren(Cursor, VisitTranslationUnit, ClientData);
                    ParserContext->PopNamespace();
                    
                    return Visitor::VisitStructure(Cursor, Parent, ParserContext);
                }

            case(CXCursor_EnumDecl):
                {
                    return Visitor::VisitEnum(Cursor, Parent, ParserContext);
                }

            case(CXCursor_Namespace):
                {
                    ParserContext->PushNamespace(CursorName);
                    clang_visitChildren(Cursor, VisitTranslationUnit, ClientData);
                    ParserContext->PopNamespace();
                }
            
            return CXChildVisit_Continue;
            
            default:
                {
                    return CXChildVisit_Continue;
                }
        }
    }
}
