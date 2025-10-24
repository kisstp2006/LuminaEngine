import os
import time
import subprocess
import sys
import shutil

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

try:
    import py7zr
    from py7zr.callbacks import ExtractCallback
except ImportError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "py7zr"])
    import py7zr
    from py7zr.callbacks import ExtractCallback


def print_header(text):
    """Print a styled header."""
    width = 80
    print("\n" + Fore.CYAN + "═" * width)
    print(Fore.CYAN + Style.BRIGHT + text.center(width))
    print(Fore.CYAN + "═" * width + "\n")


def print_step(step_num, total_steps, description):
    """Print a step indicator."""
    print(Fore.MAGENTA + Style.BRIGHT + f"\n╔═══ Step {step_num}/{total_steps} ═══╗")
    print(Fore.MAGENTA + f"║ {description}")
    print(Fore.MAGENTA + "╚" + "═" * (len(description) + 2) + "╝\n")


def print_progress_bar(progress, total, prefix='Progress:', suffix='Complete', length=50):
    """Print a progress bar."""
    if total == 0:
        return
    
    # Ensure progress doesn't exceed total
    progress = min(progress, total)
    
    try:
        filled_length = int(length * progress // total)
        bar = '█' * filled_length + '░' * (length - filled_length)
        percent = f"{100 * (progress / float(total)):.1f}"
        sys.stdout.write(f'\r{Fore.YELLOW}{prefix} |{Fore.GREEN}{bar}{Fore.YELLOW}| {percent}% {suffix}')
        sys.stdout.flush()
    except Exception:
        # Silently fail if there's any issue with progress bar
        pass


def download_from_dropbox(url, dest_filename):
    """Download a file from Dropbox with a detailed progress bar."""
    print(Fore.CYAN + Style.BRIGHT + "Connecting to Dropbox...")
    
    if "dl=0" in url:
        url = url.replace("dl=0", "dl=1")
    elif "dl=1" not in url:
        url += "?dl=1"

    try:
        response = requests.get(url, stream=True, timeout=30)
        response.raise_for_status()

        total_size = int(response.headers.get('content-length', 0))
        downloaded = 0
        chunk_size = 8192
        
        print(Fore.GREEN + f"Connection established")
        print(Fore.CYAN + f"File size: {total_size / (1024*1024):.2f} MB\n")

        start_time = time.time()
        last_update = start_time
        
        with open(dest_filename, 'wb') as f:
            for chunk in response.iter_content(chunk_size=chunk_size):
                if chunk:
                    f.write(chunk)
                    downloaded += len(chunk)
                    
                    current_time = time.time()
                    # Only update display every 0.1 seconds
                    if current_time - last_update >= 0.1 or downloaded >= total_size:
                        elapsed_time = current_time - start_time
                        if elapsed_time > 0:
                            speed = downloaded / elapsed_time / 1024 / 1024  # MB/s
                            
                            if total_size > 0:
                                print_progress_bar(downloaded, total_size, 
                                                 prefix=f'{Fore.CYAN}Downloading:',
                                                 suffix=f'{Fore.CYAN}| {speed:.2f} MB/s')
                        last_update = current_time
        
        print(Fore.GREEN + Style.BRIGHT + f"\n\nSuccessfully downloaded {dest_filename}")

    except requests.RequestException as e:
        print(Fore.RED + Style.BRIGHT + f"\nFailed to download file: {e}")
        sys.exit(1)
    except Exception as e:
        print(Fore.RED + Style.BRIGHT + f"\nUnexpected error during download: {e}")
        sys.exit(1)


def extract_7z(archive_filename, extract_to):
    """Extract a .7z archive with progress tracking."""
    if not os.path.exists(archive_filename):
        print(Fore.RED + Style.BRIGHT + f"{archive_filename} not found.")
        sys.exit(1)

    print(Fore.CYAN + Style.BRIGHT + f"Preparing to extract archive...")

    try:
        # Create a custom callback class
        class ProgressCallback(ExtractCallback):
            def __init__(self, total_files):
                self.total_files = total_files
                self.extracted_count = 0
                self.last_update = time.time()
            
            def report_start_preparation(self):
                pass
            
            def report_start(self, processing_file_path, processing_bytes):
                self.extracted_count += 1
            
            def report_update(self, decompressed_bytes):
                current_time = time.time()
                
                # Update display every 0.05 seconds or on last file
                if current_time - self.last_update >= 0.05 or self.extracted_count >= self.total_files:
                    print_progress_bar(self.extracted_count, self.total_files, 
                                     prefix=f'{Fore.CYAN}Extracting:',
                                     suffix=f'{Fore.CYAN}| {self.extracted_count}/{self.total_files} files')
                    self.last_update = current_time
                
                return super().report_update(decompressed_bytes)
            
            def report_end(self, processing_file_path, wrote_bytes):
                pass
            
            def report_warning(self, message):
                print(Fore.YELLOW + f"\n⚠ Warning: {message}")
            
            def report_postprocess(self):
                # Ensure final progress is shown
                print_progress_bar(self.total_files, self.total_files, 
                                 prefix=f'{Fore.CYAN}Extracting:',
                                 suffix=f'{Fore.CYAN}| {self.total_files}/{self.total_files} files')
        
        with py7zr.SevenZipFile(archive_filename, mode='r') as archive:
            file_list = archive.getnames()
            total_files = len(file_list)
            
            print(Fore.GREEN + f"Archive opened successfully")
            print(Fore.CYAN + f"Files to extract: {total_files}\n")
            
            # Create callback instance
            callback = ProgressCallback(total_files)
            
            # Extract all files with callback
            archive.extractall(path=extract_to, callback=callback)
            
            print(Fore.GREEN + Style.BRIGHT + f"\n\nExtraction complete! ({total_files} files extracted)")
            
    except Exception as e:
        print(Fore.RED + Style.BRIGHT + f"\nFailed to extract archive: {e}")
        sys.exit(1)


def run_generate():
    """Run the project generation script."""
    script_path = os.path.join("Scripts", "Win-GenProjects.py")

    if not os.path.exists(script_path):
        print(Fore.RED + Style.BRIGHT + f"Script not found: {script_path}")
        sys.exit(1)

    print(Fore.CYAN + Style.BRIGHT + f"Generating project files...")
    print(Fore.YELLOW + "   This may take a moment...\n")

    try:
        result = subprocess.run([sys.executable, script_path], 
                              capture_output=True, 
                              text=True,
                              encoding='utf-8',
                              errors='replace',
                              check=True)
        
        # Print the output from the script
        if result.stdout:
            print(Fore.WHITE + result.stdout)
        
        print(Fore.GREEN + Style.BRIGHT + "Project generation complete!")
        
    except subprocess.CalledProcessError as e:
        print(Fore.RED + Style.BRIGHT + f"Failed to run {script_path}")
        if e.stderr:
            print(Fore.RED + e.stderr)
        sys.exit(1)


def run_reflection():
    """Run the reflection build."""
    script_path = os.path.join("Scripts", "Reflection.py")

    if not os.path.exists(script_path):
        print(Fore.RED + Style.BRIGHT + f"Reflection script not found: {script_path}")
        sys.exit(1)

    print(Fore.CYAN + Style.BRIGHT + f"Running reflection build...")
    print(Fore.YELLOW + "   Building and running Reflector project...\n")

    try:
        # Add Scripts directory to path so we can import
        scripts_dir = os.path.join(os.getcwd(), "Scripts")
        if scripts_dir not in sys.path:
            sys.path.insert(0, scripts_dir)
        
        # Import and run the reflection module
        import Reflection
        
        # Build the reflector
        output_dir = Reflection.build_reflector(
            solution_path="Lumina.sln",
            project_name="Reflector",
            configuration="Release",
            platform="x64"
        )

        time.sleep(1.0)
        
        # Run the reflector
        Reflection.run_reflector(output_dir, "Reflector")
        
        print(Fore.GREEN + Style.BRIGHT + "Reflection complete!")
        
    except Exception as e:
        print(Fore.RED + Style.BRIGHT + f"Reflection failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


def cleanup_archive(archive_filename):
    """Remove the downloaded archive file."""
    if os.path.exists(archive_filename):
        try:
            os.remove(archive_filename)
            print(Fore.GREEN + Style.BRIGHT + f"Cleaned up {archive_filename}")
        except Exception as e:
            print(Fore.YELLOW + f"Could not remove {archive_filename}: {e}")


if __name__ == '__main__':
    try:
        print_header("LUMINA ENGINE SETUP")
        
        print(Fore.CYAN + Style.BRIGHT + "Welcome to the Lumina Engine setup script!")
        print(Fore.WHITE + "This script will download dependencies, extract files, and configure your project.\n")
        
        dropbox_url = "https://www.dropbox.com/scl/fi/suigjbqj75pzcpxcqm6hv/External.7z?rlkey=ebu8kiw4gswtvj5mclg6wa1lu&st=mndgypjl&dl=0"
        archive_filename = "External.7z"
        extract_to = "."
        
        total_steps = 6

        # Step 1: Download
        print_step(1, total_steps, "Downloading External Dependencies")
        download_from_dropbox(dropbox_url, archive_filename)
        time.sleep(0.5)

        # Step 2: Extract
        print_step(2, total_steps, "Extracting Dependencies")
        extract_7z(archive_filename, extract_to)
        time.sleep(0.5)

        # Step 3: Initial Project Generation
        print_step(3, total_steps, "Generating Project Files (Initial)")
        run_generate()
        time.sleep(0.5)

        # Step 4: Reflection Build
        print_step(4, total_steps, "Building Reflector")
        run_reflection()
        time.sleep(0.5)

        print_step(5, total_steps, "Building Editor")
        run_reflection

        # Step 5: Final Project Generation
        print_step(5, total_steps, "Generating Project Files (Final)")
        run_generate()
        time.sleep(0.5)

        # Step 6: Cleanup
        print_step(6, total_steps, "Cleaning Up")
        cleanup_archive(archive_filename)

        # Success message
        print_header("SETUP COMPLETE")
        print(Fore.GREEN + Style.BRIGHT + "Lumina Engine is ready to use!")
        print(Fore.CYAN + "\nYou can now open " + Style.BRIGHT + "Lumina.sln" + Style.NORMAL + " in Visual Studio.")
        print(Fore.YELLOW + "\nHappy coding!\n")
        
    except KeyboardInterrupt:
        print(Fore.YELLOW + "\n\n⚠ Setup interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(Fore.RED + Style.BRIGHT + f"\n✗ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)