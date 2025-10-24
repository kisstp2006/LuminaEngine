import os
import sys
import subprocess
import shutil

try:
    from colorama import Fore, Style, init
    init(autoreset=True)
except ImportError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "colorama"])
    from colorama import Fore, Style, init
    init(autoreset=True)


def find_msbuild():
    """
    Find MSBuild.exe in common Visual Studio installation paths.
    Returns the path to MSBuild.exe or None if not found.
    """
    possible_paths = [
        # VS 2022
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        r"C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        # VS 2019
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        # VS 2017
        r"C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\MSBuild.exe",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\MSBuild\15.0\Bin\MSBuild.exe",
    ]
    
    # Check if MSBuild is in PATH
    msbuild_path = shutil.which("msbuild")
    if msbuild_path:
        return msbuild_path
    
    # Check common installation paths
    for path in possible_paths:
        if os.path.exists(path):
            return path
    
    return None


def build_reflector(solution_path, project_name, configuration="Debug", platform="x64"):
    """
    Build a specific project in a Visual Studio solution using MSBuild.
    
    Args:
        solution_path: Path to the .sln file
        project_name: Name of the project to build
        configuration: Build configuration (Debug/Release)
        platform: Target platform (x64/Win32)
    """
    
    # Find MSBuild
    msbuild_path = find_msbuild()
    
    if not msbuild_path:
        print(Fore.RED + Style.BRIGHT + "MSBuild not found!")
        print(Fore.YELLOW + "\nPlease ensure Visual Studio is installed with C++ development tools.")
        print(Fore.YELLOW + "You can also add MSBuild to your system PATH.\n")
        sys.exit(1)
    
    print(Fore.GREEN + f"Found MSBuild: {msbuild_path}\n")
    
    # Check if solution exists
    if not os.path.exists(solution_path):
        print(Fore.RED + Style.BRIGHT + f"Solution not found: {solution_path}")
        sys.exit(1)
    
    # Find the project file
    # Common paths for Reflector project
    possible_project_paths = [
        os.path.join("Applications", "Reflector", f"{project_name}.vcxproj"),
        os.path.join("Lumina", "Applications", "Reflector", f"{project_name}.vcxproj"),
        os.path.join(f"{project_name}", f"{project_name}.vcxproj"),
    ]
    
    project_path = None
    project_dir = None
    for path in possible_project_paths:
        if os.path.exists(path):
            project_path = path
            project_dir = os.path.dirname(path)
            break
    
    if not project_path:
        print(Fore.RED + Style.BRIGHT + f"✗ Project file not found for {project_name}")
        print(Fore.YELLOW + "Searched in:")
        for path in possible_project_paths:
            print(Fore.YELLOW + f"  - {path}")
        sys.exit(1)
    
    # Create output directory if it doesn't exist
    # Determine the platform suffix
    platform_suffix = "x86_64" if platform.lower() == "x64" else "x86"
    output_dir = os.path.join("Binaries", f"{configuration}-windows-{platform_suffix}")
    
    if not os.path.exists(output_dir):
        print(Fore.CYAN + f"Creating output directory: {output_dir}")
        os.makedirs(output_dir, exist_ok=True)
        print(Fore.GREEN + f"Output directory created\n")
    
    print(Fore.CYAN + Style.BRIGHT + f"Building {project_name}...")
    print(Fore.WHITE + f"   Project: {project_path}")
    print(Fore.WHITE + f"   Configuration: {configuration}")
    print(Fore.WHITE + f"   Platform: {platform}")
    print(Fore.WHITE + f"   Output: {output_dir}\n")
    
    # Build the MSBuild command - build the project file directly
    cmd = [
        msbuild_path,
        project_path,
        f"/p:Configuration={configuration}",
        f"/p:Platform={platform}",
        "/m",  # Multi-processor build
        "/v:minimal"  # Minimal verbosity
    ]
    
    print(Fore.YELLOW + "Building project...\n")
    print(Fore.WHITE + "-" * 80 + "\n")
    
    try:
        # Run MSBuild
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace',
            check=True
        )
        
        # Print output
        if result.stdout:
            # Filter and colorize output
            for line in result.stdout.split('\n'):
                if 'error' in line.lower():
                    print(Fore.RED + line)
                elif 'warning' in line.lower():
                    print(Fore.YELLOW + line)
                elif 'Build succeeded' in line:
                    print(Fore.GREEN + Style.BRIGHT + line)
                elif line.strip():
                    print(Fore.WHITE + line)
        
        print(Fore.WHITE + "\n" + "-" * 80)
        print(Fore.GREEN + Style.BRIGHT + f"\nSuccessfully built {project_name}!")
        
        # Return the output directory for running the executable
        return output_dir
        
    except subprocess.CalledProcessError as e:
        print(Fore.WHITE + "\n" + "-" * 80)
        print(Fore.RED + Style.BRIGHT + f"\nBuild failed for {project_name}")
        
        if e.stderr:
            print(Fore.RED + "\nError output:")
            print(Fore.RED + e.stderr)
        
        if e.stdout:
            print(Fore.YELLOW + "\nBuild output:")
            for line in e.stdout.split('\n'):
                if 'error' in line.lower():
                    print(Fore.RED + line)
                elif line.strip():
                    print(Fore.WHITE + line)
        
        sys.exit(1)


def run_reflector(output_dir, project_name="Reflector"):
    """
    Run the Reflector executable after building.
    
    Args:
        output_dir: Directory containing the built executable
        project_name: Name of the executable (without .exe)
    """
    executable_path = os.path.join(output_dir, f"{project_name}.exe")
    
    if not os.path.exists(executable_path):
        print(Fore.RED + Style.BRIGHT + f"Executable not found: {executable_path}")
        sys.exit(1)
    
    print(Fore.CYAN + Style.BRIGHT + f"\nRunning {project_name}...")
    print(Fore.WHITE + f"   Executable: {executable_path}\n")
    print(Fore.WHITE + "-" * 80 + "\n")
    
    try:
        # Run the reflector executable
        result = subprocess.run(
            [executable_path],
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace',
            check=True,
            cwd=output_dir  # Run from the output directory
        )
        
        # Print output
        if result.stdout:
            for line in result.stdout.split('\n'):
                if 'error' in line.lower():
                    print(Fore.RED + line)
                elif 'warning' in line.lower():
                    print(Fore.YELLOW + line)
                elif 'success' in line.lower() or 'complete' in line.lower():
                    print(Fore.GREEN + line)
                elif line.strip():
                    print(Fore.WHITE + line)
        
        print(Fore.WHITE + "\n" + "-" * 80)
        print(Fore.GREEN + Style.BRIGHT + f"\n{project_name} completed successfully!")
        
        return True
        
    except subprocess.CalledProcessError as e:
        print(Fore.WHITE + "\n" + "-" * 80)
        print(Fore.RED + Style.BRIGHT + f"\n✗ {project_name} failed")
        
        if e.stderr:
            print(Fore.RED + "\nError output:")
            print(Fore.RED + e.stderr)
        
        if e.stdout:
            print(Fore.YELLOW + "\nOutput:")
            for line in e.stdout.split('\n'):
                if line.strip():
                    print(Fore.WHITE + line)
        
        sys.exit(1)


if __name__ == '__main__':
    # Configuration
    solution_file = "Lumina.sln"
    reflector_project = "Reflector"
    build_config = "Debug"
    build_platform = "x64"
    
    print(Fore.CYAN + Style.BRIGHT + "═" * 80)
    print(Fore.CYAN + Style.BRIGHT + "LUMINA REFLECTOR BUILD & RUN".center(80))
    print(Fore.CYAN + Style.BRIGHT + "═" * 80 + "\n")
    
    # Build the Reflector project
    output_dir = build_reflector(
        solution_path=solution_file,
        project_name=reflector_project,
        configuration=build_config,
        platform=build_platform
    )
    
    # Run the Reflector executable
    run_reflector(output_dir, reflector_project)
    
    print(Fore.CYAN + "\n" + "═" * 80)
    print(Fore.GREEN + Style.BRIGHT + "Reflection process complete!".center(80))
    print(Fore.CYAN + "═" * 80 + "\n")