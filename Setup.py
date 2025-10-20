import os
import time
import subprocess
import sys

try:
    from colorama import Fore, Style, init
except ImportError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "colorama"])
    from colorama import Fore, Style, init

try:
    import requests
except ImportError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "requests"])
    import requests

try:
    import py7zr
except ImportError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "py7zr"])
    import py7zr


def download_from_dropbox(url, dest_filename):
    """
    Download a file from Dropbox with a progress bar.
    """
    print(Fore.CYAN + f"[INFO] Downloading from Dropbox: {url}")

    # Ensure direct download
    if "dl=0" in url:
        url = url.replace("dl=0", "dl=1")
    elif "dl=1" not in url:
        url += "?dl=1"

    try:
        response = requests.get(url, stream=True)
        response.raise_for_status()

        total_size = int(response.headers.get('content-length', 0))
        downloaded = 0
        chunk_size = 8192

        with open(dest_filename, 'wb') as f:
            for chunk in response.iter_content(chunk_size=chunk_size):
                if chunk:
                    f.write(chunk)
                    downloaded += len(chunk)
                    if total_size:
                        percent = downloaded / total_size * 100
                        print(Fore.YELLOW + f"\r[DOWNLOAD] {percent:.2f}% ({downloaded}/{total_size} bytes)", end='')
        print(Fore.GREEN + f"\n[SUCCESS] Downloaded {dest_filename}")

    except requests.RequestException as e:
        print(Fore.RED + f"[ERROR] Failed to download file: {e}")
        sys.exit(1)


def extract_7z(archive_filename, extract_to):
    """
    Extract a .7z archive using py7zr.
    """
    init(autoreset=True)

    if not os.path.exists(archive_filename):
        print(Fore.RED + f"[ERROR] {archive_filename} not found.")
        sys.exit(1)

    print(Fore.CYAN + f"[INFO] Extracting {archive_filename} to {extract_to}...")

    try:
        with py7zr.SevenZipFile(archive_filename, mode='r') as archive:
            file_list = archive.getnames()
            print(Fore.YELLOW + f"[INFO] Extracting {len(file_list)} files...")
            
            archive.extractall(path=extract_to)
            
            print(Fore.GREEN + f"[SUCCESS] Extraction complete!")
    except Exception as e:
        print(Fore.RED + f"[ERROR] Failed to extract archive: {e}")
        sys.exit(1)
    finally:
        print(Style.RESET_ALL)


def run_generate():
    """
    Run the project generation script.
    """
    init(autoreset=True)

    script_path = os.path.join("Scripts", "Win-GenProjects.py")

    if not os.path.exists(script_path):
        print(Fore.RED + f"[ERROR] Script not found: {script_path}")
        sys.exit(1)

    print(Fore.CYAN + f"[INFO] Generating project files...")

    try:
        subprocess.run([sys.executable, script_path], check=True)
        print(Fore.GREEN + "[SUCCESS] Project generation complete!")
    except subprocess.CalledProcessError as e:
        print(Fore.RED + f"[ERROR] Failed to run {script_path}: {e}")
        sys.exit(1)


if __name__ == '__main__':
    dropbox_url = "https://www.dropbox.com/scl/fi/suigjbqj75pzcpxcqm6hv/External.7z?rlkey=ebu8kiw4gswtvj5mclg6wa1lu&st=mndgypjl&dl=0"
    archive_filename = "External.7z"  # Changed from .zip to .7z
    extract_to = "."  # Current directory

    # Download the file
    download_from_dropbox(dropbox_url, archive_filename)

    time.sleep(1)
    
    # Extract the 7z archive
    extract_7z(archive_filename, extract_to)

    # Run the generation script
    run_generate()