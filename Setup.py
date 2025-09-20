import zipfile
import os
import time
import subprocess
try:
    from colorama import Fore, Style, init
except ImportError:
    import subprocess
    import sys
    subprocess.check_call([sys.executable, "-m", "pip", "install", "colorama"])
    from colorama import Fore, Style, init

try:
    import requests
except ImportError:
    import subprocess
    import sys
    subprocess.check_call([sys.executable, "-m", "pip", "install", "requests"])
    import requests


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


    
def extract_zip(zip_filename, extract_to):
    init(autoreset=True)  # Initialize colorama for colored output

    if not os.path.exists(zip_filename):
        print(Fore.RED + f"[ERROR] {zip_filename} not found.")
        return

    print(Fore.CYAN + f"[INFO] Extracting {zip_filename} to {extract_to}...")

    try:
        with zipfile.ZipFile(zip_filename, 'r') as zip_ref:
            file_list = zip_ref.namelist()
            for file in file_list:
                print(Fore.YELLOW + f"[EXTRACTING] {file}")
            zip_ref.extractall(extract_to)
        print(Fore.GREEN + f"[SUCCESS] Extraction complete!")
    finally:
        print(Style.RESET_ALL)


def run_generate():
    init(autoreset=True)

    script_path = os.path.join("Scripts", "Win-GenProjects.py")

    if not os.path.exists(script_path):
        print(Fore.RED + f"[ERROR] Script not found: {script_path}")
        return

    print(Fore.CYAN + f"[INFO] Generating project files...")

    try:
        subprocess.run(["python", script_path], check=True)
    except subprocess.CalledProcessError as e:
        print(Fore.RED + f"[ERROR] Failed to run {script_path}: {e}")


if __name__ == '__main__':
    dropbox_url = "https://www.dropbox.com/scl/fi/10e1d9vj994zsisvco284/External.zip?rlkey=oy9oqz9r67g8wlappch7tlk8i&st=lrksgn5s&dl=1"
    zip_filename = "External.zip"
    extract_to = ""

    download_from_dropbox(dropbox_url, zip_filename)

    extract_zip(zip_filename, extract_to)
    time.sleep(1)

    run_generate()
