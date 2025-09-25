#pragma once


namespace Lumina
{
    struct FNeedsTransformUpdate { uint8 Foobar:1; };
    template<typename T> struct FNeedsRenderState            {};
    template<typename T> struct FNeedsRenderStateUpdated     {};
    template<typename T> struct FNeedsRenderStateDestroyed   {};
    template<typename T> struct FNeedsRenderTransformUpdated {};
}
