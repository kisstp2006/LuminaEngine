<img src="https://github.com/user-attachments/assets/552b8ca0-ebca-4876-9c6a-df38c468d41e" width="120"/>

# Lumina Game Engine

<table>
  <tr>
    <td><img src="https://img.shields.io/github/license/mrdrelliot/lumina"></td>
    <td><img src="https://img.shields.io/badge/status-WIP-orange"></td>
    <td><img src="https://img.shields.io/badge/platform-Windows-blue"></td>
  </tr>
  <tr>
    <td><img src="https://img.shields.io/badge/build-Visual%20Studio%202022-blueviolet"></td>
    <td><img src="https://img.shields.io/badge/language-C++23-blue"></td>
    <td><img src="https://img.shields.io/badge/renderer-Vulkan-red"></td>
  </tr>
</table>

![Discord](https://img.shields.io/discord/1193738186892005387?link=https%3A%2F%2Fdiscord.gg%2F4ZjwrB3c9A)

**Lumina** is a C++ game engine powered by Vulkan, built as a hands-on learning project. Inspired by engines like Unreal and Godot, it focuses on reflection, modularity, and a clean ImGui-based editor. The goal isn’t to become the next major engine or to optimize anyone’s workflow, but to serve as a passion project, a way to explore and experiment with the architectures that make up a full-fledged game engine.

Up to date blog: https://www.dr-elliot.com

# ⚠️ DISCLAIMER

> **Early development** – APIs and systems are in flux, and not everything is fully documented or stable yet. **THE PROJECT MOST LIKELY WON'T BUILD FOR YOU, IF YOU WOULD LIKE TO BUILD IT, PLEASE REACH OUT TO GET HELP**

---

## ✨ Features

- 🔁 **Reflection system** for serialization and editor integration
- 🖥️ **ImGui-based editor** with real-time scene editing
- 🎮 **ECS-Based workflow**.
- 🎮 **Vulkan renderer**
---

## 📸 Screenshots

<img width="1912" height="552" alt="image" src="https://github.com/user-attachments/assets/8c81055c-f46a-447d-a79c-31b51fded805" />
<img width="2229" height="1222" alt="image" src="https://github.com/user-attachments/assets/a6b973ba-851e-4732-b30b-eb0bf14b08e1" />
<img width="2225" height="1222" alt="image" src="https://github.com/user-attachments/assets/944c2569-a969-42b9-b0e6-88050fb5037c" />
<img width="2222" height="1222" alt="image" src="https://github.com/user-attachments/assets/b8717096-e8e9-437b-af18-01502ed821b9" />
<img width="2228" height="1227" alt="image" src="https://github.com/user-attachments/assets/6b1dc6a7-ffb3-416b-93ad-d130695e810e" />
<img width="2323" height="961" alt="image" src="https://github.com/user-attachments/assets/9974246c-4bc0-4975-b489-5846f1551c74" />



---

# 🛠️ Building Lumina (Windows Only, please see disclaimer near top)

> ✅ MSVC 17.8+ required  
> ❌ Linux/macOS support is not available yet

### Setup:

1. Clone the repo.
2. Run `Setup.py` (this extracts external dependencies).
3. Ensure you install VulkanMemoryAllocator whilst installing Vulkan.
4. Ensure the `LUMINA_DIR` environment variable is set to the engine install directory file path (e.g. C:/CoolStuff/LuminaGameEngine)
5. Run `Scripts/Win-GenProjects.py`.
6. Open the generated `.sln` in Visual Studio.
7. Build & Run the **Reflector** in `Release` mode.
8. Set **Editor** as startup project. Select Debug or Release mode.
9. Build & run.

---

## 🧱 Included Applications

- **Editor** – ImGui-based main app
- **Sandbox** – For API testing
- **Reflector** – Generates reflection metadata

---

## 📦 Requirements

- **Visual Studio 2022**
- **Python** (auto-installed if missing)
- **Vulkan SDK**

---

## 🧭 Roadmap

- ✅ Windows support
- 🔄 Refactor to dynamic/shared libraries
- 🔄 Scene batched rendering
- 🔜 Multithreaded renderer
- 🔜 Plugin system
- 🔜 macOS/Linux support

---

## 🤝 Contributing

Pull requests are welcome!

> See [CONTRIBUTING.md](CONTRIBUTING.md) for code style and guidelines.

- ✅ Tests, documentation, and clean commits required.

---

## 🧑‍💻 Coding Style

- `F` prefix = internal engine types (non-reflected)
- `C` prefix = reflected types (classes)
- `S` prefix = reflected types (structs)
- PascalCase for all identifiers
- Tabs (not spaces), braces on new lines
- See [Coding Standards](CONTRIBUTING.md) for full details.

---

## 🛠️ Integrated Third-Party Libraries

| Library           | Purpose/Description                                      |
|-------------------|----------------------------------------------------------|
| **GLFW**          | OpenGL, Vulkan, and window management                    |
| **VMA**           | Vulkan Memory Allocator                                  |
| **EASTL**         | High-performance C++ standard library replacement         |
| **VkBootstrap**   | Helper library for Vulkan initialization                 |
| **EnTT**          | Entity Component System (ECS) framework                  |
| **EnkiTS**        | Task scheduler for parallelism and multi-threading       |
| **ImGui**         | Immediate mode GUI framework                              |
| **FastGLTF**      | GLTF 2.0 parser and writer                               |
| **GLM**           | Mathematics library for graphics (similar to GLSL)       |
| **NlohmannJson**  | JSON for Modern C++ (for reading/writing JSON data)      |
| **RPMalloc**      | High-performance memory allocator                        |
| **SPDLog**        | Fast, header-only logging library                        |
| **SPIRV-Reflect** | SPIR-V reflection library for Vulkan                     |
| **STB_Image**     | Image loading library (supports multiple formats)        |
| **Tracy**         | Real-time, low-overhead profiler                         |
| **XXHash**        | Extremely fast non-cryptographic hash function           |

---

## 📄 License

Lumina is licensed under [Apache 2.0](LICENSE).

You may use, modify, and distribute the code freely — just keep the license and attribution intact.

---

## 🔗 Links

- [GitHub Repository](https://github.com/mrdrelliot/lumina)
- [Discord Community](https://discord.gg/xQSB7CRzQE)

