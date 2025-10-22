#pragma once
#include "imgui.h"
#include "Containers/String.h"
#include "Core/UpdateContext.h"
#include "Containers/Function.h"
#include <Memory/SmartPtr.h>

namespace Lumina
{
    class FEditorToolModal;
    class FUpdateContext;
}

namespace Lumina
{

    class FEditorModalManager
    {
    public:

        ~FEditorModalManager();

        void CreateDialogue(const FString& Title, ImVec2 Size, TMoveOnlyFunction<bool(const FUpdateContext&)> DrawFunction, bool bBlocking = true);

        FORCEINLINE bool HasModal() const { return ActiveModal.get() != nullptr; }

        void DrawDialogue(const FUpdateContext& UpdateContext);

    private:
        
        TUniquePtr<FEditorToolModal>       ActiveModal;
        
    };
    
    class FEditorToolModal
    {
    public:

        friend class FEditorModalManager;
        
        virtual ~FEditorToolModal() = default;

        FEditorToolModal(const FString& InTitle, ImVec2 InSize)
            : DrawFunction()
            , Title(InTitle)
            , Size(InSize)
        {}

        /** Return true to indicate the modal is ready to close */
        bool DrawModal(const FUpdateContext& UpdateContext)
        {
            return DrawFunction(UpdateContext);
        }
        
    protected:

        TMoveOnlyFunction<bool(const FUpdateContext&)>  DrawFunction;
        FString                                         Title;
        ImVec2                                          Size;
        bool                                            bBlocking;
    
    };
}
