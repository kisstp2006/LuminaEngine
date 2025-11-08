#include "ObjectReferenceReplacerArchive.h"


namespace Lumina
{
    FArchive& FObjectReferenceReplacerArchive::operator<<(CObject*& Obj)
    {
        if (Obj && Obj == ToReplace)
        {
            if (ToReplace)
            {
                GObjectArray.AddStrongRef(ToReplace);
            }
            
            GObjectArray.ReleaseStrongRef(Obj);
            Obj = Replacement;
        }

        return *this;
    }
}
