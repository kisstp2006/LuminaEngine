import os
import sys
import json
import shutil
import subprocess
from pathlib import Path
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from typing import Optional
import threading


# Auto-install required packages
def install_packages():
    """Install required packages if not present."""
    required = ['colorama']
    missing = []
    
    for package in required:
        try:
            __import__(package)
        except ImportError:
            missing.append(package)
    
    if missing:
        print(f"Installing required packages: {', '.join(missing)}")
        subprocess.check_call([sys.executable, "-m", "pip", "install", *missing])

install_packages()

from colorama import Fore, Style, init
init()


class DarkTheme:
    """Dark theme color scheme."""
    BG_DARK = "#1e1e1e"
    BG_MEDIUM = "#2d2d2d"
    BG_LIGHT = "#3e3e3e"
    FG_PRIMARY = "#ffffff"
    FG_SECONDARY = "#b0b0b0"
    ACCENT = "#007acc"
    ACCENT_HOVER = "#1c97ea"
    SUCCESS = "#4ec9b0"
    ERROR = "#f48771"
    WARNING = "#dcdcaa"
    BORDER = "#555555"


class LuminaProjectCreator:
    """Main application class for Lumina Project Creator."""
    
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Lumina Project Creator")
        self.root.geometry("650x550")
        self.root.resizable(False, False)
        
        # Apply dark theme
        self.setup_theme()
        
        # Variables
        self.project_path = tk.StringVar()
        self.project_name = tk.StringVar()
        self.status_text = tk.StringVar(value="Ready to create a new project")
        
        # Get Lumina engine directory
        self.lumina_dir = os.getenv("LUMINA_DIR")
        
        # Setup UI
        self.setup_ui()
        self.check_engine_directory()
    
    def setup_theme(self):
        """Configure dark theme for the application."""
        self.root.configure(bg=DarkTheme.BG_DARK)
        
        style = ttk.Style()
        style.theme_use('clam')
        
        # Configure ttk widgets
        style.configure(".", 
            background=DarkTheme.BG_DARK,
            foreground=DarkTheme.FG_PRIMARY,
            bordercolor=DarkTheme.BORDER,
            darkcolor=DarkTheme.BG_MEDIUM,
            lightcolor=DarkTheme.BG_LIGHT,
            troughcolor=DarkTheme.BG_MEDIUM,
            focuscolor=DarkTheme.ACCENT,
            selectbackground=DarkTheme.ACCENT,
            selectforeground=DarkTheme.FG_PRIMARY,
            fieldbackground=DarkTheme.BG_MEDIUM,
            font=("Segoe UI", 9)
        )
        
        style.configure("TFrame", background=DarkTheme.BG_DARK)
        style.configure("TLabel", background=DarkTheme.BG_DARK, foreground=DarkTheme.FG_PRIMARY)
        style.configure("TButton", 
            background=DarkTheme.BG_LIGHT,
            foreground=DarkTheme.FG_PRIMARY,
            bordercolor=DarkTheme.BORDER,
            focuscolor=DarkTheme.ACCENT,
            padding=6
        )
        style.map("TButton",
            background=[("active", DarkTheme.BG_MEDIUM), ("pressed", DarkTheme.BG_DARK)]
        )
        
        style.configure("Accent.TButton",
            background=DarkTheme.ACCENT,
            foreground=DarkTheme.FG_PRIMARY,
            font=("Segoe UI", 10, "bold"),
            padding=8
        )
        style.map("Accent.TButton",
            background=[("active", DarkTheme.ACCENT_HOVER), ("pressed", DarkTheme.ACCENT)]
        )
        
        style.configure("TEntry",
            fieldbackground=DarkTheme.BG_MEDIUM,
            foreground=DarkTheme.FG_PRIMARY,
            bordercolor=DarkTheme.BORDER,
            insertcolor=DarkTheme.FG_PRIMARY
        )
        
        style.configure("TLabelframe",
            background=DarkTheme.BG_DARK,
            foreground=DarkTheme.FG_PRIMARY,
            bordercolor=DarkTheme.BORDER
        )
        style.configure("TLabelframe.Label",
            background=DarkTheme.BG_DARK,
            foreground=DarkTheme.FG_SECONDARY,
            font=("Segoe UI", 9, "bold")
        )
        
        style.configure("Horizontal.TProgressbar",
            troughcolor=DarkTheme.BG_MEDIUM,
            background=DarkTheme.ACCENT,
            bordercolor=DarkTheme.BORDER,
            lightcolor=DarkTheme.ACCENT,
            darkcolor=DarkTheme.ACCENT
        )
        
        style.configure("Vertical.TScrollbar",
            background=DarkTheme.BG_LIGHT,
            troughcolor=DarkTheme.BG_MEDIUM,
            bordercolor=DarkTheme.BORDER,
            arrowcolor=DarkTheme.FG_PRIMARY
        )
    
    def setup_ui(self):
        """Create the user interface."""
        # Main container with padding
        main_frame = ttk.Frame(self.root, padding="20")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        
        # Title
        title_label = ttk.Label(
            main_frame, 
            text="Lumina Project Creator",
            font=("Segoe UI", 18, "bold"),
            foreground=DarkTheme.FG_PRIMARY
        )
        title_label.grid(row=0, column=0, columnspan=3, pady=(0, 20))
        
        # Engine Directory Section
        engine_frame = ttk.LabelFrame(main_frame, text="Engine Configuration", padding="10")
        engine_frame.grid(row=1, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 15))
        engine_frame.columnconfigure(1, weight=1)
        
        ttk.Label(engine_frame, text="Engine Directory:").grid(row=0, column=0, sticky=tk.W, padx=(0, 10))
        self.engine_label = ttk.Label(
            engine_frame, 
            text="Not Set", 
            foreground=DarkTheme.ERROR,
            font=("Segoe UI", 9)
        )
        self.engine_label.grid(row=0, column=1, sticky=tk.W)
        
        # Project Configuration Section
        config_frame = ttk.LabelFrame(main_frame, text="Project Configuration", padding="10")
        config_frame.grid(row=2, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 15))
        config_frame.columnconfigure(1, weight=1)
        
        # Project Name
        ttk.Label(config_frame, text="Project Name:").grid(row=0, column=0, sticky=tk.W, pady=5, padx=(0, 10))
        name_entry = ttk.Entry(config_frame, textvariable=self.project_name, font=("Segoe UI", 10))
        name_entry.grid(row=0, column=1, sticky=(tk.W, tk.E), pady=5, padx=(0, 10))
        name_entry.focus()
        
        # Project Directory
        ttk.Label(config_frame, text="Project Directory:").grid(row=1, column=0, sticky=tk.W, pady=5, padx=(0, 10))
        path_entry = ttk.Entry(config_frame, textvariable=self.project_path, font=("Segoe UI", 9))
        path_entry.grid(row=1, column=1, sticky=(tk.W, tk.E), pady=5, padx=(0, 10))
        
        browse_btn = ttk.Button(config_frame, text="Browse...", command=self.browse_directory, width=12)
        browse_btn.grid(row=1, column=2, pady=5)
        
        # Progress Section
        progress_frame = ttk.Frame(main_frame)
        progress_frame.grid(row=3, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 15))
        progress_frame.columnconfigure(0, weight=1)
        
        self.progress_bar = ttk.Progressbar(progress_frame, mode='determinate', length=400)
        self.progress_bar.grid(row=0, column=0, sticky=(tk.W, tk.E), pady=(0, 5))
        
        status_label = ttk.Label(
            progress_frame, 
            textvariable=self.status_text,
            font=("Segoe UI", 9),
            foreground=DarkTheme.FG_SECONDARY
        )
        status_label.grid(row=1, column=0, sticky=tk.W)
        
        # Buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.grid(row=4, column=0, columnspan=3, pady=(10, 0))
        
        self.create_btn = ttk.Button(
            button_frame, 
            text="Create Project",
            command=self.create_project,
            style="Accent.TButton",
            width=20
        )
        self.create_btn.grid(row=0, column=0, padx=5)
        
        ttk.Button(
            button_frame,
            text="Exit",
            command=self.root.quit,
            width=15
        ).grid(row=0, column=1, padx=5)
        
        # Console Output
        console_frame = ttk.LabelFrame(main_frame, text="Output Log", padding="5")
        console_frame.grid(row=5, column=0, columnspan=3, sticky=(tk.W, tk.E, tk.N, tk.S), pady=(15, 0))
        console_frame.columnconfigure(0, weight=1)
        console_frame.rowconfigure(0, weight=1)
        main_frame.rowconfigure(5, weight=1)
        
        self.console = tk.Text(
            console_frame, 
            height=8, 
            wrap=tk.WORD, 
            font=("Consolas", 9),
            bg=DarkTheme.BG_MEDIUM,
            fg=DarkTheme.FG_PRIMARY,
            insertbackground=DarkTheme.FG_PRIMARY,
            selectbackground=DarkTheme.ACCENT,
            selectforeground=DarkTheme.FG_PRIMARY,
            borderwidth=0,
            highlightthickness=1,
            highlightbackground=DarkTheme.BORDER,
            highlightcolor=DarkTheme.ACCENT,
            state='disabled'
        )
        self.console.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        scrollbar = ttk.Scrollbar(console_frame, orient=tk.VERTICAL, command=self.console.yview)
        scrollbar.grid(row=0, column=1, sticky=(tk.N, tk.S))
        self.console['yscrollcommand'] = scrollbar.set
    
    def check_engine_directory(self):
        """Check if Lumina engine directory is set."""
        if self.lumina_dir and os.path.exists(self.lumina_dir):
            self.engine_label.config(text=self.lumina_dir, foreground=DarkTheme.SUCCESS)
            self.log(f"Engine directory found: {self.lumina_dir}", "success")
        else:
            self.engine_label.config(text="Not found! Set LUMINA_DIR environment variable", foreground=DarkTheme.ERROR)
            self.log("Engine directory not found. Please set LUMINA_DIR environment variable.", "error")
            self.create_btn.config(state='disabled')
    
    def browse_directory(self):
        """Open directory browser."""
        directory = filedialog.askdirectory(title="Select Project Directory")
        if directory:
            self.project_path.set(directory)
    
    def log(self, message: str, level: str = "info"):
        """Add message to console output."""
        self.console.config(state='normal')
        
        if level == "success":
            prefix = "✓ "
            color = DarkTheme.SUCCESS
        elif level == "error":
            prefix = "✗ "
            color = DarkTheme.ERROR
        elif level == "warning":
            prefix = "⚠ "
            color = DarkTheme.WARNING
        else:
            prefix = "• "
            color = DarkTheme.FG_SECONDARY
        
        # Insert the prefix with color
        start_pos = self.console.index(tk.END)
        self.console.insert(tk.END, prefix)
        end_prefix = self.console.index(tk.END)
        self.console.tag_add(f"prefix_{level}", start_pos, end_prefix)
        self.console.tag_config(f"prefix_{level}", foreground=color)
        
        # Insert the message
        self.console.insert(tk.END, f"{message}\n")
        
        self.console.see(tk.END)
        self.console.config(state='disabled')
        self.root.update_idletasks()
    
    def update_progress(self, value: int, status: str):
        """Update progress bar and status."""
        self.progress_bar['value'] = value
        self.status_text.set(status)
        self.root.update_idletasks()
    
    def sanitize_filename(self, filename: str) -> str:
        """Sanitize filename by replacing spaces and trimming."""
        return filename.replace(" ", "_").strip()
    
    def validate_inputs(self) -> bool:
        """Validate user inputs."""
        if not self.project_name.get().strip():
            messagebox.showerror("Invalid Input", "Please enter a project name.")
            return False
        
        if not self.project_path.get().strip():
            messagebox.showerror("Invalid Input", "Please select a project directory.")
            return False
        
        if not self.lumina_dir or not os.path.exists(self.lumina_dir):
            messagebox.showerror("Engine Not Found", "Lumina engine directory not found. Please set LUMINA_DIR environment variable.")
            return False
        
        return True
    
    def create_project_json(self, folder: str, name: str) -> str:
        """Create the project JSON file."""
        file_path = os.path.join(folder, f"{name}.lproject")
        project_data = {"ProjectName": name}
        
        try:
            with open(file_path, "w") as f:
                json.dump(project_data, f, indent=4)
            self.log(f"Created project file: {name}.lproject", "success")
            return file_path
        except Exception as e:
            self.log(f"Error creating project JSON: {e}", "error")
            raise
    
    def create_premake_files(self, folder: str, name: str):
        """Create premake configuration files."""
        template_path = os.path.join(self.lumina_dir, "Templates/Project/premake_solution_template.txt")
        output_path = os.path.join(folder, "premake5.lua")
        
        with open(template_path) as f:
            content = f.read().replace("$PROJECT_NAME", name)
        
        with open(output_path, "w") as f:
            f.write(content)
        
        self.log("Created premake5.lua", "success")
    
    def create_python_files(self, folder: str):
        """Create Python build script."""
        script_content = '''import subprocess
import os

def generate_project():
    # Call premake5 to generate Visual Studio solution
    subprocess.call(["Tools/premake5.exe", "vs2022"])

if __name__ == "__main__":
    generate_project()
'''
        
        file_path = os.path.join(folder, "GenerateProject.py")
        with open(file_path, "w") as f:
            f.write(script_content)
        
        self.log("Created GenerateProject.py", "success")
    
    def create_module_header(self, folder: str, name: str):
        """Create module header file."""
        name = self.sanitize_filename(name)
        file_path = os.path.join(folder, f"Source/{name}/{name}.h")
        
        content = f'''#pragma once
#include "Core/Module/ModuleInterface.h"

// This is your core game module that the engine loads.
class F{name} : public Lumina::IModuleInterface
{{
    //...
}};
'''
        
        os.makedirs(os.path.dirname(file_path), exist_ok=True)
        with open(file_path, "w") as f:
            f.write(content)
        
        self.log(f"Created {name}.h", "success")
    
    def create_module_source(self, folder: str, name: str):
        """Create module source file."""
        name = self.sanitize_filename(name)
        file_path = os.path.join(folder, f"Source/{name}/{name}.cpp")
        
        content = f'''#include "{name}.h"
#include "Core/Module/ModuleManager.h"

// Boilerplate module discovery and implementation.
IMPLEMENT_MODULE(F{name}, "{name}")
'''
        
        os.makedirs(os.path.dirname(file_path), exist_ok=True)
        with open(file_path, "w") as f:
            f.write(content)
        
        self.log(f"Created {name}.cpp", "success")
        
    def create_module_api(self, folder: str, name: str):
        """Create module API file."""
        name = self.sanitize_filename(name)
        source_dir = os.path.join(folder, f"Source/{name}")
        output_path = os.path.join(source_dir, "API.h")
        template_path = os.path.join(self.lumina_dir, "Templates/Project/api_template.txt")
    
        # Ensure the directory exists
        os.makedirs(source_dir, exist_ok=True)
    
        with open(template_path) as f:
            content = f.read().replace("$PROJECT_NAME", name.upper())
    
        with open(output_path, "w") as f:
            f.write(content)
        
        self.log("Created API.h", "success")

    
    def create_project_directories(self, directory: str, name: str):
        """Create project directory structure."""
        os.makedirs(directory, exist_ok=True)
        
        source_dir = os.path.join(directory, f"Source/{name}")
        content_dir = os.path.join(directory, "Game/Content")
        
        os.makedirs(source_dir, exist_ok=True)
        os.makedirs(content_dir, exist_ok=True)
        
        self.log("Created project directories", "success")
    
    def create_eastl_template(self, directory: str, name: str):
        """Create EASTL template file."""
        source_dir = os.path.join(directory, f"Source/{name}")
        template_path = os.path.join(self.lumina_dir, "Templates/Project/eastl_template.txt")
        output_path = os.path.join(source_dir, f"{name}_eastl.cpp")
        
        with open(template_path) as f:
            content = f.read()
        
        with open(output_path, "w") as f:
            f.write(content)
        
        self.log(f"Created {name}_eastl.lua", "success")
    
    def copy_tools_directory(self, folder: str):
        """Copy Tools directory from engine."""
        src = os.path.join(self.lumina_dir, "Tools")
        dest = os.path.join(folder, "Tools")
        
        try:
            shutil.copytree(src, dest)
            self.log("Copied Tools directory", "success")
        except Exception as e:
            self.log(f"Error copying Tools: {e}", "error")
            raise
    


    def create_project(self):
        """Main project creation workflow."""
        if not self.validate_inputs():
            return
        
        # Disable button during creation
        self.create_btn.config(state='disabled')
        self.progress_bar['value'] = 0
        
        try:
            name = self.sanitize_filename(self.project_name.get())
            base_folder = self.project_path.get()
            
            # Create project folder inside the selected directory
            folder = os.path.join(base_folder, name)
            
            # Check if project folder already exists
            if os.path.exists(folder):
                messagebox.showerror("Error", f"Project folder '{name}' already exists at this location!")
                self.create_btn.config(state='normal')
                return
            
            self.log(f"Creating project '{name}' at {folder}...", "info")
            
            # Create project structure
            steps = [
                (10, "Creating directories...", lambda: self.create_project_directories(folder, name)),
                (20, "Creating module header...", lambda: self.create_module_header(folder, name)),
                (30, "Creating module source...", lambda: self.create_module_source(folder, name)),
                (40, "Creating Python files...", lambda: self.create_python_files(folder)),
                (50, "Creating project JSON...", lambda: self.create_project_json(folder, name)),
                (60, "Creating EASTL template...", lambda: self.create_eastl_template(folder, name)),
                (60, "Creating API template...", lambda: self.create_module_api(folder, name)),
                (70, "Creating premake files...", lambda: self.create_premake_files(folder, name)),
                (90, "Copying Tools directory...", lambda: self.copy_tools_directory(folder)),
            ]
            
            for progress, status, action in steps:
                self.update_progress(progress, status)
                action()
            
            self.update_progress(100, "Running project generation...")
            self.log("Running GenerateProject.py...", "info")
                        
            # Run the GenerateProject.py script
            generate_script = os.path.join(folder, "GenerateProject.py")
            try:
                # Run in the project directory
                result = subprocess.run(
                    [sys.executable, generate_script],
                    cwd=folder,
                    capture_output=True,
                    text=True
                )
                
                if result.returncode == 0:
                    self.log("Project generation completed successfully!", "success")
                else:
                    self.log(f"Project generation warning: {result.stderr}", "warning")
                    
            except Exception as e:
                self.log(f"Error running GenerateProject.py: {e}", "warning")
            
            self.update_progress(100, "Project created successfully!")
            self.log(f"Project '{name}' created successfully!", "success")
            
            messagebox.showinfo("Success", f"Project '{name}' has been created successfully!\n\nLocation: {folder}")
            
            os.startfile(folder)
            
        except Exception as e:
            self.log(f"Failed to create project: {str(e)}", "error")
            messagebox.showerror("Error", f"Failed to create project:\n{str(e)}")
        finally:
            self.create_btn.config(state='normal')


def main():
    """Application entry point."""
    root = tk.Tk()
    app = LuminaProjectCreator(root)
    root.mainloop()


if __name__ == "__main__":
    main()