#pragma once


namespace Lumina::Physics
{
    class IPhysicsScene
    {
    public:

        virtual ~IPhysicsScene() { }
        virtual void PreUpdate() = 0;
        virtual void Update(double DeltaTime) = 0;
        virtual void PostUpdate() = 0;
        virtual void OnWorldSimulate() = 0;
        virtual void OnWorldStopSimulate() = 0;

        
    };
}