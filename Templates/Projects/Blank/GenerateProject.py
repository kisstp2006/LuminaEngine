import subprocess
import os

def generate_project():
    """Generate Visual Studio solution using premake5."""
    print("Generating project files...")
    subprocess.call(["Tools/premake5.exe", "vs2022"])
    print("Project generation complete!")

if __name__ == "__main__":
    generate_project()