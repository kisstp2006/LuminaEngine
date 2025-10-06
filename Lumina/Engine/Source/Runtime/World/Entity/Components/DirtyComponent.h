#pragma once


namespace Lumina
{
    struct LUMINA_API FNeedsTransformUpdate { uint8 Foobar:1; };
    struct FNeedsRenderState            {};
    struct FNeedsRenderStateUpdated     {};
    struct FNeedsRenderStateDestroyed   {};
    struct FNeedsRenderTransformUpdated {};
}
