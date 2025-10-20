"""
CheckPython.py - Validates and installs required Python packages for Lumina Engine
Modern implementation without deprecated pkg_resources
"""

import subprocess
import sys
from importlib import metadata
from importlib.util import find_spec

# Required packages for Lumina
REQUIRED_PACKAGES = {
    'requests': 'requests',
    'colorama': 'colorama',
}

def is_package_installed(package_name):
    """Check if a package is installed using importlib.metadata."""
    try:
        metadata.version(package_name)
        return True
    except metadata.PackageNotFoundError:
        return False

def can_import(module_name):
    """Check if a module can be imported."""
    return find_spec(module_name) is not None

def install_package(package_name):
    """Install a package using pip."""
    print(f"Installing {package_name} module...")
    try:
        subprocess.check_call(
            [sys.executable, "-m", "pip", "install", package_name, "-q"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE
        )
        return True
    except subprocess.CalledProcessError as e:
        print(f"Failed to install {package_name}: {e}")
        return False

def ValidatePackages():
    """
    Validate that all required packages are installed.
    Install them if they're missing.
    """
    missing_packages = []
    
    # Check each required package
    for package_name, import_name in REQUIRED_PACKAGES.items():
        if not is_package_installed(package_name) and not can_import(import_name):
            missing_packages.append(package_name)
    
    # Install missing packages
    if missing_packages:
        print(f"Installing {len(missing_packages)} missing package(s)...")
        for package in missing_packages:
            if not install_package(package):
                raise RuntimeError(f"Failed to install required package: {package}")
    
    # Verify all packages are now available
    for package_name, import_name in REQUIRED_PACKAGES.items():
        if not can_import(import_name):
            raise RuntimeError(f"Package {package_name} still not available after installation")
    
    return True

def CheckPythonVersion():
    """Check if Python version meets minimum requirements."""
    MIN_PYTHON = (3, 8)
    
    if sys.version_info < MIN_PYTHON:
        print(f"Error: Python {MIN_PYTHON[0]}.{MIN_PYTHON[1]} or higher is required")
        print(f"Current version: {sys.version}")
        return False
    
    return True

if __name__ == "__main__":
    """Can be run standalone to verify requirements."""
    print("Checking Python environment for Lumina Engine...")
    
    if not CheckPythonVersion():
        sys.exit(1)
    
    try:
        ValidatePackages()
        print("✓ All requirements satisfied")
        print(f"✓ Python {sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}")
        
        # List installed versions
        for package_name in REQUIRED_PACKAGES.keys():
            try:
                version = metadata.version(package_name)
                print(f"✓ {package_name} {version}")
            except:
                pass
                
    except Exception as e:
        print(f"✗ Validation failed: {e}")
        sys.exit(1)