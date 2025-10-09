import os
import subprocess
import sys
import platform

# --- Ensure Python packages ---
try:
    import CheckPython
except ImportError:
    print("CheckPython module missing. Make sure it's in Scripts/ directory.")
    input("Press Enter to exit...")
    sys.exit(1)

try:
    import colorama
except ImportError:
    subprocess.check_call([sys.executable, "-m", "ensurepip", "--upgrade"])
    subprocess.check_call([sys.executable, "-m", "pip", "install", "colorama"])
    import colorama

from colorama import Fore, Back, Style

# --- Initialization ---
colorama.init()
os.chdir('../')  # Move from Scripts/ to project root
print(f"{Style.BRIGHT}Generating Project Files for Lumina\n{Style.RESET_ALL}")

# --- Constants ---
VULKAN_SDK_URL = "https://vulkan.lunarg.com/sdk/home"
VULKAN_ENV_VAR = "VULKAN_SDK"
VULKAN_INSTALL_DIR = os.path.join(os.getcwd(), "Dependencies", "Vulkan")

# --- Set Environment Variable ---
current_dir = os.getcwd()
print(f"{Style.BRIGHT}Setting LUMINA_DIR to {current_dir}{Style.RESET_ALL}")
os.environ['LUMINA_DIR'] = current_dir

# Persist for future processes
subprocess.call(["setx", "LUMINA_DIR", current_dir], shell=True)

# --- Check Python Requirements ---
try:
    # This will also auto-install missing packages if you fixed CheckPython.py as discussed
    CheckPython.ValidatePackages()
except Exception as e:
    print(f"{Fore.RED}Error: Required Python packages not installed. {e}{Style.RESET_ALL}")
    input("\nPress Enter to exit...")
    sys.exit(1)

# --- Check Vulkan SDK ---
def is_vulkan_installed():
    return os.environ.get(VULKAN_ENV_VAR) is not None or os.path.exists(VULKAN_INSTALL_DIR)

if not is_vulkan_installed():
    print(f"{Fore.RED}Vulkan SDK not found!{Style.RESET_ALL}")
    print(f"{Fore.CYAN}Please install it from: {VULKAN_SDK_URL}{Style.RESET_ALL}")
    input("\nPress Enter to exit...")
    sys.exit(1)
else:
    vk_path = os.environ.get(VULKAN_ENV_VAR, VULKAN_INSTALL_DIR)
    print(f"{Fore.GREEN}Vulkan SDK found at: {vk_path}{Style.RESET_ALL}")

# --- Generate Visual Studio Solution ---
print(f"\n{Style.BRIGHT}{Back.GREEN}Generating Visual Studio 2022 solution...{Style.RESET_ALL}")

premake_path = os.path.join("Tools", "premake5.exe")
if not os.path.exists(premake_path):
    print(f"{Fore.RED}Premake executable not found at {premake_path}!{Style.RESET_ALL}")
    input("\nPress Enter to exit...")
    sys.exit(1)

try:
    subprocess.check_call([premake_path, "vs2022"])
    print(f"{Fore.GREEN}Solution generated successfully!{Style.RESET_ALL}")
except subprocess.CalledProcessError:
    print(f"{Fore.RED}Premake generation failed. Please check your setup.{Style.RESET_ALL}")

input("\nPress Enter to exit...")
