#pragma once


namespace Lumina
{
    /** A scene is a version of a world that is responsible for managing a part of that world.
     * An example would be rendering, or physics.
     * **/
    class ISceneInterface
    {
    public:
        
        virtual void Init() = 0;
        virtual void Shutdown() = 0;
    };
}