


#include "pch.h"
#include "Window.h"

#include "stb_image.h"
#include "Core/Application/Application.h"
#include "Events/ApplicationEvent.h"
#include "Paths/Paths.h"
#include "Platform/Platform.h"
#include "Renderer/RHIIncl.h"


namespace
{
	void GLFWErrorCallback(int error, const char* description)
	{
		LOG_CRITICAL("GLFW Error: {0} | {1}", error, description);
	}
	
	void* CustomGLFWAllocate(size_t size, void* user)
	{
		return Lumina::Memory::Malloc(size);
	}

	void* CustomGLFWReallocate(void* block, size_t size, void* user)
	{
		return Lumina::Memory::Realloc(block, size);
	}

	void CustomGLFWDeallocate(void* block, void* user)
	{
		Lumina::Memory::Free(block);
	}

	GLFWallocator CustomAllocator =
	{
		CustomGLFWAllocate,
		CustomGLFWReallocate,
		CustomGLFWDeallocate,
		nullptr
	};
}


namespace Lumina
{

	FWindowDropEvent FWindow::OnWindowDropped;
	FWindowResizeEvent FWindow::OnWindowResized;
	
	GLFWmonitor* GetCurrentMonitor(GLFWwindow* window)
	{
		int windowX, windowY, windowWidth, windowHeight;
		glfwGetWindowPos(window, &windowX, &windowY);
		glfwGetWindowSize(window, &windowWidth, &windowHeight);

		int monitorCount;
		GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

		GLFWmonitor* bestMonitor = nullptr;
		int maxOverlap = 0;

		for (int i = 0; i < monitorCount; ++i)
		{
			int monitorX, monitorY, monitorWidth, monitorHeight;
			glfwGetMonitorWorkarea(monitors[i], &monitorX, &monitorY, &monitorWidth, &monitorHeight);

			int overlapX = std::max(0, std::min(windowX + windowWidth, monitorX + monitorWidth) - std::max(windowX, monitorX));
			int overlapY = std::max(0, std::min(windowY + windowHeight, monitorY + monitorHeight) - std::max(windowY, monitorY));
			int overlapArea = overlapX * overlapY;

			if (overlapArea > maxOverlap)
			{
				maxOverlap = overlapArea;
				bestMonitor = monitors[i];
			}
		}

		return bestMonitor;
	}
	
	FWindow::~FWindow()
	{
		Assert(Window == nullptr)
	}

	void FWindow::Init()
	{
		if (LIKELY(!bInitialized))
		{
			glfwInitAllocator(&CustomAllocator);
			glfwInit();
			glfwSetErrorCallback(GLFWErrorCallback);
			
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_TITLEBAR, false);

			Window = glfwCreateWindow(800, 400, Specs.Title.c_str(), nullptr, nullptr);
			glfwSetWindowAttrib(Window, GLFW_RESIZABLE, GLFW_TRUE);
			
			if (GLFWmonitor* currentMonitor = GetCurrentMonitor(Window))
			{
				int monitorX, monitorY, monitorWidth, monitorHeight;
				glfwGetMonitorWorkarea(currentMonitor, &monitorX, &monitorY, &monitorWidth, &monitorHeight);

				if (Specs.Extent.x == 0 || Specs.Extent.x >= monitorWidth)
				{
					Specs.Extent.x = static_cast<decltype(Specs.Extent.x)>(static_cast<float>(monitorWidth) / 1.15f);
				}
				if (Specs.Extent.y == 0 || Specs.Extent.y >= monitorHeight)
				{
					Specs.Extent.y = static_cast<decltype(Specs.Extent.y)>(static_cast<float>(monitorHeight) / 1.15f);
				}
				

				LOG_TRACE("Initializing Window: {0} (Width: {1}p Height: {2}p)", Specs.Title, Specs.Extent.x, Specs.Extent.y);
			}

			glfwSetWindowSize(Window, Specs.Extent.x, Specs.Extent.y);

			GLFWimage Icon;
			int Channels;
			FString IconPathStr = Paths::GetEngineResourceDirectory() + "/Textures/Lumina.png";
			Icon.pixels = stbi_load(IconPathStr.c_str(), &Icon.width, &Icon.height, &Channels, 4);
			glfwSetWindowIcon(Window, 1, &Icon);
			stbi_image_free(Icon.pixels);
			
			glfwSetWindowUserPointer(Window, this);
			glfwSetWindowSizeCallback(Window, WindowResizeCallback);
			glfwSetDropCallback(Window, WindowDropCallback);
			glfwSetWindowCloseCallback(Window, WindowCloseCallback);
		}
	}
	
	void FWindow::Shutdown()
	{
		glfwDestroyWindow(Window);
		Window = nullptr;
		
		glfwTerminate();
	}

	void FWindow::ProcessMessages()
	{
		glfwPollEvents();
	}

	bool FWindow::IsWindowMaximized() const
	{
		return glfwGetWindowAttrib(Window, GLFW_MAXIMIZED);
	}

	void FWindow::GetWindowPosition(int& X, int& Y)
	{
		glfwGetWindowPos(Window, &X, &Y);
	}

	void FWindow::SetWindowPosition(int X, int Y)
	{
		glfwSetWindowPos(Window, X, Y);
	}

	void FWindow::SetWindowSize(int X, int Y)
	{
		glfwSetWindowSize(Window, X, Y);
	}

	bool FWindow::ShouldClose() const
	{
		return glfwWindowShouldClose(Window);
	}

	bool FWindow::IsWindowMinimized() const
	{
		return glfwGetWindowAttrib(Window, GLFW_ICONIFIED);
	}


	void FWindow::Minimize()
	{
		glfwIconifyWindow(Window);
	}

	void FWindow::Restore()
	{
		glfwRestoreWindow(Window);
	}

	void FWindow::Maximize()
	{
		glfwMaximizeWindow(Window);
	}

	void FWindow::Close()
	{
		glfwSetWindowShouldClose(Window, GLFW_TRUE);
	}

	void FWindow::WindowResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto WindowHandle = (FWindow*)glfwGetWindowUserPointer(window);
		WindowHandle->Specs.Extent.x = width;
		WindowHandle->Specs.Extent.y = height;
		OnWindowResized.Broadcast(WindowHandle, WindowHandle->Specs.Extent);
	}

	void FWindow::WindowDropCallback(GLFWwindow* Window, int PathCount, const char* Paths[])
	{
		auto WindowHandle = (FWindow*)glfwGetWindowUserPointer(Window);

		OnWindowDropped.Broadcast(WindowHandle, PathCount, Paths);
	}

	void FWindow::WindowCloseCallback(GLFWwindow* window)
	{
		FApplication::RequestExit();
	}

	FWindow* FWindow::Create(const FWindowSpecs& InSpecs)
	{
		return Memory::New<FWindow>(InSpecs);
	}

	namespace Windowing
	{
		FWindow* PrimaryWindow;
		
		FWindow* GetPrimaryWindowHandle()
		{
			Assert(PrimaryWindow != nullptr)
			return PrimaryWindow;
		}

		void SetPrimaryWindowHandle(FWindow* InWindow)
		{
			Assert(PrimaryWindow == nullptr)
			PrimaryWindow = InWindow;
		}
	}
}
