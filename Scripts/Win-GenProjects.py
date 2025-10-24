import os
import subprocess
import sys
import time
import platform
import zipfile
import shutil
from pathlib import Path

# --- Ensure Python packages ---
def ensure_package(package_name, import_name=None):
    """Install package if missing."""
    if import_name is None:
        import_name = package_name
    
    try:
        __import__(import_name)
    except ImportError:
        print(f"Installing {package_name}...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", package_name, "-q"])

ensure_package("colorama")
ensure_package("requests")

try:
    from colorama import Fore, Style, Back, init
    init(autoreset=True)
except ImportError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "colorama"])
    from colorama import Fore, Style, Back, init
    init(autoreset=True)


try:
    import requests
except ImportError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "requests"])
    import requests


# --- Constants ---
VULKAN_SDK_URL = "https://vulkan.lunarg.com/sdk/home"
VULKAN_ENV_VAR = "VULKAN_SDK"
PREMAKE_VERSION = "5.0.0-beta2"
PREMAKE_URL = f"https://github.com/premake/premake-core/releases/download/v{PREMAKE_VERSION}/premake-{PREMAKE_VERSION}-windows.zip"

# --- Fancy Header ---
def print_header():
    border = "=" * 70
    print(f"\n{Fore.CYAN}{Style.BRIGHT}{border}")
    print(f"{Fore.WHITE}{Style.BRIGHT}{'LUMINA ENGINE - PROJECT GENERATOR':^70}")
    print(f"{Fore.CYAN}{Style.BRIGHT}{border}{Style.RESET_ALL}\n")

def print_section(title):
    """Print a section header."""
    print(f"\n{Fore.YELLOW}{Style.BRIGHT} - {title}{Style.RESET_ALL}")
    print(f"{Fore.YELLOW}{'-' * (len(title) + 2)}{Style.RESET_ALL}")

def print_success(message):
    """Print a success message."""
    print(f"{Fore.GREEN} {message}{Style.RESET_ALL}")

def print_error(message):
    """Print an error message."""
    print(f"{Fore.RED} {message}{Style.RESET_ALL}")

def print_info(message):
    """Print an info message."""
    print(f"{Fore.CYAN} {message}{Style.RESET_ALL}")

def print_warning(message):
    """Print a warning message."""
    print(f"{Fore.YELLOW} {message}{Style.RESET_ALL}")

# --- Download with Progress ---
def download_file(url, dest_path, description):
    """Download a file with a progress bar."""
    print_info(f"Downloading {description}...")
    
    try:
        response = requests.get(url, stream=True)
        response.raise_for_status()
        
        total_size = int(response.headers.get('content-length', 0))
        downloaded = 0
        chunk_size = 8192
        
        with open(dest_path, 'wb') as f:
            for chunk in response.iter_content(chunk_size=chunk_size):
                if chunk:
                    f.write(chunk)
                    downloaded += len(chunk)
                    if total_size:
                        percent = (downloaded / total_size) * 100
                        bar_length = 40
                        filled = int(bar_length * downloaded / total_size)
                        bar = '█' * filled + '░' * (bar_length - filled)
                        print(f"\r{Fore.CYAN}[{bar}] {percent:.1f}% ({downloaded}/{total_size} bytes){Style.RESET_ALL}", end='')
        
        print()  # New line after progress bar
        print_success(f"Downloaded {description}")
        return True
        
    except requests.RequestException as e:
        print()
        print_error(f"Failed to download {description}: {e}")
        return False

# --- Extract Archive ---
def extract_zip(zip_path, extract_to):
    """Extract a zip file."""
    print_info(f"Extracting {os.path.basename(zip_path)}...")
    
    try:
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            zip_ref.extractall(extract_to)
        print_success(f"Extracted to {extract_to}")
        return True
    except Exception as e:
        print_error(f"Failed to extract: {e}")
        return False

# --- Setup Premake ---
def setup_premake():
    """Download and setup Premake5 if missing."""
    print_section("Premake5 Setup")
    
    tools_dir = Path("Tools")
    tools_dir.mkdir(exist_ok=True)
    
    premake_exe = tools_dir / "premake5.exe"
    
    if premake_exe.exists():
        print_success(f"Premake5 found at {premake_exe}")
        return str(premake_exe)
    
    print_warning("Premake5 not found. Downloading...")
    
    # Download premake
    temp_zip = tools_dir / "premake.zip"
    if not download_file(PREMAKE_URL, str(temp_zip), "Premake5"):
        return None
    
    # Extract premake
    if not extract_zip(str(temp_zip), str(tools_dir)):
        return None
    
    # Clean up
    temp_zip.unlink()
    
    if premake_exe.exists():
        print_success(f"Premake5 installed at {premake_exe}")
        return str(premake_exe)
    else:
        print_error("Premake5 installation failed")
        return None

# --- Check Vulkan SDK ---
def check_vulkan():
    """Check if Vulkan SDK is installed."""
    print_section("Vulkan SDK Verification")
    
    vulkan_sdk = os.environ.get(VULKAN_ENV_VAR)
    
    if vulkan_sdk and os.path.exists(vulkan_sdk):
        print_success(f"Vulkan SDK found at: {vulkan_sdk}")
        return True
    
    # Check common install locations
    common_paths = [
        os.path.join(os.getcwd(), "Dependencies", "Vulkan"),
        "C:\\VulkanSDK",
    ]
    
    for path in common_paths:
        if os.path.exists(path):
            print_success(f"Vulkan SDK found at: {path}")
            os.environ[VULKAN_ENV_VAR] = path
            return True
    
    print_error("Vulkan SDK not found!")
    print_info(f"Please install from: {Fore.BLUE}{VULKAN_SDK_URL}")
    print_info("After installation, restart this script.")
    return False

# --- Set Environment Variable ---
def set_lumina_dir():
    """Set LUMINA_DIR environment variable."""
    print_section("Environment Setup")
    
    current_dir = os.getcwd()
    os.environ['LUMINA_DIR'] = current_dir
    
    print_info(f"Setting LUMINA_DIR to: {current_dir}")
    
    try:
        if platform.system() == "Windows":
            subprocess.run(["setx", "LUMINA_DIR", current_dir], 
                         capture_output=True, check=True)
        print_success("Environment variable set successfully")
    except subprocess.CalledProcessError:
        print_warning("Failed to persist environment variable (may require admin rights)")
        print_info("Variable is set for this session only")

# --- Generate Visual Studio Solution ---
def generate_solution(premake_path):
    """Generate Visual Studio solution using Premake."""
    print_section("Generating Visual Studio 2022 Solution")
    
    print_info("Running Premake5...")
    
    try:
        result = subprocess.run(
            [premake_path, "vs2022"],
            capture_output=True,
            text=True,
            check=True
        )
        
        print_success("Solution generated successfully!")
        
        # Find and display the .sln file
        sln_files = list(Path(".").glob("*.sln"))
        if sln_files:
            print_info(f"Solution file: {Fore.WHITE}{Style.BRIGHT}{sln_files[0].name}")
        
        return True
        
    except subprocess.CalledProcessError as e:
        print_error("Premake generation failed!")
        if e.stderr:
            print(f"{Fore.RED}{e.stderr}{Style.RESET_ALL}")
        return False

# --- Check Python Modules ---
def check_python_requirements():
    """Check if CheckPython module exists and run it."""
    print_section("Python Requirements Check")
    
    try:
        import CheckPython
        CheckPython.ValidatePackages()
        print_success("All Python requirements satisfied")
        return True
    except ImportError:
        print_warning("CheckPython module not found (optional)")
        return True  # Non-critical
    except Exception as e:
        print_error(f"Python validation failed: {e}")
        return False

# --- Main Execution ---
def main():
    """Main setup routine."""
    print_header()
    
    # Change to project root
    if os.path.basename(os.getcwd()) == "Scripts":
        os.chdir('../')
        print_info(f"Working directory: {os.getcwd()}\n")
    
    # Step 1: Set environment
    set_lumina_dir()
    
    # Step 2: Check Python
    if not check_python_requirements():
        print_error("\nSetup failed: Python requirements not met")
        time.sleep(1.5)
        return 1
    
    # Step 3: Check Vulkan
    if not check_vulkan():
        print_error("\nSetup failed: Vulkan SDK required")
        time.sleep(1.5)
        return 1
    
    # Step 4: Setup Premake
    premake_path = setup_premake()
    if not premake_path:
        print_error("\nSetup failed: Could not install Premake5")
        time.sleep(1.5)
        return 1
    
    # Step 5: Generate solution
    if not generate_solution(premake_path):
        print_error("\nSetup failed: Solution generation failed")
        time.sleep(1.5)
        return 1
    
    # Success!
    print(f"\n{Fore.GREEN}{Style.BRIGHT}{'=' * 70}")
    print(f"{Fore.GREEN}{Style.BRIGHT}{'SETUP COMPLETE!':^70}")
    print(f"{Fore.GREEN}{Style.BRIGHT}{'=' * 70}{Style.RESET_ALL}\n")
    print_info("You can now open the solution file in Visual Studio 2022")
    time.sleep(0.25)
    print_success("This window will close in 2 seconds...")
    time.sleep(2.0)

    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print(f"\n\n{Fore.YELLOW}Setup cancelled by user{Style.RESET_ALL}")
        sys.exit(1)
    except Exception as e:
        print(f"\n{Fore.RED}{Style.BRIGHT}Unexpected error: {e}{Style.RESET_ALL}")
        import traceback
        traceback.print_exc()
        sys.exit(1)