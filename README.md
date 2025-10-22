<div align="center">

<img src="https://github.com/user-attachments/assets/552b8ca0-ebca-4876-9c6a-df38c468d41e" width="120"/>

# Lumina Game Engine

**A modern, high-performance game engine built with Vulkan**

[![License](https://img.shields.io/github/license/mrdrelliot/lumina)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-blue)](https://github.com/mrdrelliot/lumina)
[![C++](https://img.shields.io/badge/C++-23-blue)](https://github.com/mrdrelliot/lumina)
[![Vulkan](https://img.shields.io/badge/Vulkan-renderer-red)](https://www.vulkan.org/)
[![Discord](https://img.shields.io/discord/1193738186892005387?label=Discord&logo=discord)](https://discord.gg/xQSB7CRzQE)

[Blog](https://www.dr-elliot.com) • [Discord](https://discord.gg/xQSB7CRzQE) • [Documentation](#-documentation)

</div>

---

## 🎯 About

Lumina is a modern C++ game engine designed for learning and experimentation with real-world engine architecture. Built from the ground up with Vulkan, it demonstrates professional engine design patterns including reflection systems, ECS architecture, and advanced rendering techniques.

**Perfect for:**
- 🎓 Learning modern game engine architecture
- 🔬 Experimenting with Vulkan rendering techniques
- 🛠️ Building prototypes with a clean, modular codebase
- 📚 Understanding how engines like Unreal and Godot work under the hood

> **Note:** Lumina is an educational project in active development. APIs may change, and some features are experimental. If you encounter build issues, please reach out on [Discord](https://discord.gg/xQSB7CRzQE) for assistance.

---

## ✨ Key Features

### 🎨 **Advanced Rendering**
- **Vulkan-powered renderer** with automatic resource tracking and barrier placement
- **Deferred rendering pipeline** with clustered lighting for efficient multi-light scenes
- **PBR materials** with full GLTF/GLB support

### 🏗️ **Modern Architecture**
- **Entity Component System (ECS)** using EnTT for high-performance gameplay code
- **Reflection system** for automatic serialization and editor integration
- **Modular design** with clean separation of concerns

### 🖥️ **Professional Editor**
- **ImGui-based editor** with real-time scene manipulation
- **Visual hierarchy** for easy entity management
- **Component inspector** with automatic UI generation via reflection

### ⚡ **Performance First**
- **Multi-threaded task system** with EnkiTS
- **Custom memory allocators** using RPMalloc for optimal performance
- **Built-in profiling** with Tracy integration

---

## 📸 Gallery

<div align="center">

<img src="https://github.com/user-attachments/assets/a6b973ba-851e-4732-b30b-eb0bf14b08e1" alt="Scene Editor" width="800"/>

*Real-time scene editing with ImGui-based editor*

<img src="https://github.com/user-attachments/assets/944c2569-a969-42b9-b0e6-88050fb5037c" alt="Clustered Lighting" width="800"/>

*Deferred rendering with clustered lighting*

<img src="https://github.com/user-attachments/assets/b8717096-e8e9-437b-af18-01502ed821b9" alt="PBR Materials" width="800"/>

*Physically-based materials with GLTF support*

</div>

<details>
<summary>📷 View More Screenshots</summary>

<img src="https://github.com/user-attachments/assets/8c81055c-f46a-447d-a79c-31b51fded805" alt="Editor Overview"/>
<img src="https://github.com/user-attachments/assets/6b1dc6a7-ffb3-416b-93ad-d130695e810e" alt="Component Inspector"/>
<img src="https://github.com/user-attachments/assets/9974246c-4bc0-4975-b489-5846f1551c74" alt="Scene Hierarchy"/>
<img src="https://github.com/user-attachments/assets/a5659962-8c9b-4bf7-9730-6ebc079b42fd" alt="Material Editor"/>
<img src="https://github.com/user-attachments/assets/9ccd1e52-bb64-44c8-bd8c-66c1f1545253" alt="Lighting System"/>

</details>

---

## 🚀 Quick Start

### Prerequisites

- **Windows 10/11** (64-bit)
- **Visual Studio** (MSVC 17.8+)
- **Python 3.8+** (automatically installed if missing)
- **Vulkan SDK**  ([Download](https://vulkan.lunarg.com/sdk/home))

### Installation

```bash
# 1. Clone the repository
git clone https://github.com/mrdrelliot/lumina.git
cd lumina

# 2. Run setup (downloads dependencies automatically)
python Setup.py

# 3. Generate Visual Studio solution
python Scripts/Win-GenProjects.py

# 4. Build the Reflector (required for reflection system)
# Open Lumina.sln → Build Reflector project in Release mode

# 5. Regenerate solution with reflection data
python Scripts/Win-GenProjects.py

# 6. Build and run the Editor
# Set Editor as startup project → Build → Run
```

### First Time Setup

After building, the `LUMINA_DIR` environment variable should be automatically set. If not:
```bash
setx LUMINA_DIR "C:\path\to\lumina"
```

---

## 📦 Project Structure

```
Lumina/
├── Editor/          # Main editor application
├── Sandbox/         # Testing and experimentation playground
├── Reflector/       # Reflection metadata generator
├── Engine/          # Core engine modules
│   ├── Renderer/    # Vulkan rendering system
│   ├── ECS/         # Entity Component System
│   ├── Core/        # Foundation systems
│   └── ...
├── Scripts/         # Build and automation scripts
└── External/        # Third-party dependencies
```

---

## 🎮 Supported Asset Formats

| Format | Support | Notes |
|--------|---------|-------|
| **GLTF** | ✅ Full | Recommended format |
| **GLB** | ✅ Full | Binary GLTF |
| **PNG/JPG** | ✅ Full | Textures via STB_Image |

### Free Asset Resources

- [Khronos GLTF Samples](https://github.com/KhronosGroup/glTF-Sample-Assets)
- [Kenney 3D Assets](https://kenney.nl/assets?q=3d)
- [Flightradar24 Models](https://github.com/Flightradar24/fr24-3d-models)

---

## 🛠️ Technology Stack

<table>
<tr>
<td valign="top" width="50%">

### Core Systems
- **GLFW** - Window & input management
- **EnTT** - Entity Component System
- **EnkiTS** - Multi-threaded task scheduler
- **EASTL** - High-performance STL replacement
- **RPMalloc** - Custom memory allocator

</td>
<td valign="top" width="50%">

### Rendering
- **Vulkan** - Graphics API
- **VMA** - Vulkan Memory Allocator
- **VkBootstrap** - Vulkan initialization
- **SPIRV-Reflect** - Shader reflection
- **GLM** - Math library

</td>
</tr>
<tr>
<td valign="top" width="50%">

### Tools & Utilities
- **ImGui** - Editor UI framework
- **Tracy** - Performance profiler
- **SPDLog** - Fast logging
- **NlohmannJson** - JSON serialization
- **XXHash** - Fast hashing

</td>
<td valign="top" width="50%">

### Content Pipeline
- **FastGLTF** - GLTF 2.0 parser
- **STB_Image** - Image loading
- **Reflection** - Custom C++ reflection

</td>
</tr>
</table>

---

## 📖 Documentation

### Coding Standards

Lumina follows a consistent naming convention:

| Prefix | Usage | Example |
|--------|-------|---------|
| `F` | Internal engine types (non-reflected) | `FRenderer`, `FTexture` |
| `C` | Reflected classes | `CTransform`, `CMeshRenderer` |
| `S` | Reflected structs | `SVertex`, `SMaterial` |

**General Rules:**
- ✅ PascalCase for all identifiers
- ✅ Tabs for indentation
- ✅ Braces on new lines
- ✅ Descriptive variable names

See [CONTRIBUTING.md](CONTRIBUTING.md) for complete guidelines.

### Architecture Overview

```
Game Loop → ECS Update → Render Graph → Vulkan Commands → Present
              ↓              ↓              ↓
          Reflection    Scene Graph    Resource Manager
```

---

## 🗺️ Roadmap

### Current Focus
- 🔄 **Refactoring to dynamic libraries** - Better modularity and faster iteration
- 🔄 **Scene batched rendering** - Improved draw call efficiency
- 🔄 **Documentation expansion** - API docs and tutorials

### Upcoming Features
- ⏳ **Multi-threaded renderer** - Parallel command buffer generation
- ⏳ **Plugin system** - Hot-reloadable game code
- ⏳ **Animation system** - Skeletal animation support
- ⏳ **Physics integration** - Rigid body dynamics
- ⏳ **Audio system** - 3D spatial audio

### Future Platforms
- 📋 **macOS support** - Metal backend
- 📋 **Linux support** - Native Vulkan

---

## 🤝 Contributing

Contributions are welcome! Whether it's bug fixes, features, or documentation improvements.

### How to Contribute
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes following the [coding standards](CONTRIBUTING.md)
4. Add tests if applicable
5. Commit with clear messages (`git commit -m 'Add amazing feature'`)
6. Push to your branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

**Requirements:**
- ✅ Clean, well-documented code
- ✅ Follow existing architecture patterns
- ✅ Include tests where appropriate
- ✅ Update documentation as needed

---

## 🙏 Acknowledgments

Lumina is inspired by and learns from these excellent open-source engines:

- [**Spartan Engine**](https://github.com/PanosK92/SpartanEngine) - Feature-rich Vulkan engine
- [**Kohi Game Engine**](https://kohiengine.com/) - Educational engine series
- [**ezEngine**](https://ezengine.net/) - Professional open-source engine

Special thanks to the entire game engine development community for sharing knowledge and resources.

---

## 📄 License

Lumina is licensed under the [Apache 2.0 License](LICENSE).

```
Copyright 2024 Dr. Elliot

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

---

## 🔗 Connect

- 🌐 **Blog**: [dr-elliot.com](https://www.dr-elliot.com)
- 💬 **Discord**: [Join our community](https://discord.gg/xQSB7CRzQE)
- 🐙 **GitHub**: [mrdrelliot/lumina](https://github.com/mrdrelliot/lumina)

---

<div align="center">

**Made with ❤️ for the game development community**

⭐ Star this repo if you find it useful!

</div>
