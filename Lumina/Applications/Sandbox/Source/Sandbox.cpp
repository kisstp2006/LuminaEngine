
#include "Sandbox.h"
#include "EntryPoint.h"
#include "Lumina_eastl.cpp"
#include "Assets/AssetManager/AssetManager.h"
#include "Core/Module/ModuleManager.h"
#include "Core/Object/Class.h"
#include "Core/Reflection/Type/LuminaTypes.h"
#include "Input/Input.h"

using namespace Lumina;


Lumina::FEngine* FSandbox::CreateEngine()
{
	FSandboxEngine* Engine = Memory::New<FSandboxEngine>();
	Engine->Init(this);
	return Engine;
}

void FSandboxEngine::OnUpdateStage(const Lumina::FUpdateContext& Context)
{
	LOG_INFO("asdjasdklaskld");
}

bool FSandbox::ApplicationLoop()
{
	return true;
}

bool FSandbox::Initialize(int argc, char** argv)
{
	return true;
}

void FSandbox::Shutdown()
{
}

Lumina::FApplication* Lumina::CreateApplication(int argc, char** argv)
{
	return new FSandbox();
}

DECLARE_MODULE_ALLOCATOR_OVERRIDES()
