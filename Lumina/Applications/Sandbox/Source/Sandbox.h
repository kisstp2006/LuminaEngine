#pragma once

#include "Core/Engine/Engine.h"
#include "Core/Object/Object.h"
#include "Source/Runtime/Core/Application/Application.h"


class FSandboxEngine : public Lumina::FEngine
{
public:
	
	IDevelopmentToolUI* CreateDevelopmentTools() override { return nullptr; }
	void OnUpdateStage(const Lumina::FUpdateContext& Context) override;
private:
	
};


class FSandbox : public Lumina::FApplication
{
public:

	FSandbox() :FApplication("Sandbox") {}

	Lumina::FEngine* CreateEngine() override;
	bool ApplicationLoop() override;
	bool Initialize(int argc, char** argv) override;
	void Shutdown() override;


private:
	
};
