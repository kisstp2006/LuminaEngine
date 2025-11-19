#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "WindowTypes.h"
#include "Core/Delegates/Delegate.h"
#include "Core/Math/Math.h"


namespace Lumina
{
	class FWindow;

	LUMINA_API DECLARE_MULTICAST_DELEGATE(FWindowDropEvent, FWindow*, int, const char**);
	LUMINA_API DECLARE_MULTICAST_DELEGATE(FWindowResizeEvent, FWindow*, const glm::uvec2&);
	
	class FWindow
	{
	public:
		
		static FWindow* Create(const FWindowSpecs& InSpecs);

		FWindow(const FWindowSpecs& InSpecs)
			:Specs(InSpecs)
		{}
		 
		virtual ~FWindow();


		void Init();
		void Shutdown();
		void ProcessMessages();

		GLFWwindow* GetWindow() const { return Window; }

		LUMINA_API const glm::uvec2& GetExtent() const { return Specs.Extent; }
		LUMINA_API uint32 GetWidth() const { return Specs.Extent.x; }
		LUMINA_API uint32 GetHeight() const { return Specs.Extent.y; }

		LUMINA_API void GetWindowPosition(int& X, int& Y);
		LUMINA_API void SetWindowPosition(int X, int Y);
		
		LUMINA_API void SetWindowSize(int X, int Y);

		LUMINA_API bool ShouldClose() const;
		LUMINA_API bool IsWindowMinimized() const;
		LUMINA_API bool IsWindowMaximized() const;
		LUMINA_API void Minimize();
		LUMINA_API void Restore();
		LUMINA_API void Maximize();
		LUMINA_API void Close();

		static void WindowResizeCallback(GLFWwindow* window, int width, int height);
		static void WindowDropCallback(GLFWwindow* Window, int PathCount, const char* Paths[]);
		static void WindowCloseCallback(GLFWwindow* window);
		
		LUMINA_API static FWindowDropEvent OnWindowDropped;
		LUMINA_API static FWindowResizeEvent OnWindowResized;

	private:

		bool bInitialized = false;
		GLFWwindow* Window = nullptr;
		FWindowSpecs Specs;
	};

	namespace Windowing
	{
		LUMINA_API extern FWindow* PrimaryWindow;
		LUMINA_API FWindow* GetPrimaryWindowHandle();
		void SetPrimaryWindowHandle(FWindow* InWindow);
	}
	
}
