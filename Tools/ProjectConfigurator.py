import os
import sys
import json
import shutil
import subprocess
from pathlib import Path
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from typing import Optional, List
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


class TemplateManager:
    """Manages project templates."""
    
    def __init__(self, lumina_dir: str):
        self.lumina_dir = lumina_dir
        self.templates_dir = os.path.join(lumina_dir, "Templates", "Projects")
    
    def get_available_templates(self) -> List[str]:
        """Get list of available project templates."""
        if not os.path.exists(self.templates_dir):
            return []
        
        templates = []
        for item in os.listdir(self.templates_dir):
            template_path = os.path.join(self.templates_dir, item)
            if os.path.isdir(template_path):
                templates.append(item)
        
        return sorted(templates)
    
    def get_template_path(self, template_name: str) -> Optional[str]:
        """Get the full path to a template."""
        template_path = os.path.join(self.templates_dir, template_name)
        if os.path.exists(template_path):
            return template_path
        return None
    
    def get_template_info(self, template_name: str) -> dict:
        """Get template metadata from template.json if it exists."""
        template_path = self.get_template_path(template_name)
        if not template_path:
            return {"name": template_name, "description": "No description available"}
        
        info_file = os.path.join(template_path, "template.json")
        if os.path.exists(info_file):
            try:
                with open(info_file, 'r') as f:
                    return json.load(f)
            except:
                pass
        
        return {"name": template_name, "description": "No description available"}


class ProjectCreator:
    """Handles project creation from templates."""
    
    def __init__(self, lumina_dir: str, log_callback):
        self.lumina_dir = lumina_dir
        self.log = log_callback
        self.replacements = {}
    
    def set_project_info(self, project_name: str, project_path: str):
        """Set project information for replacements."""
        self.project_name = project_name
        self.project_path = project_path
        self.replacements = {
            "${PROJECT_NAME}": project_name,
            "${PROJECT_NAME_UPPER}": project_name.upper(),
            "${PROJECT_NAME_LOWER}": project_name.lower(),
            "$PROJECT_NAME": project_name,
        }
    
    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for replacements."""
        text_extensions = {'.h', '.cpp', '.c', '.py', '.lua', '.json', '.txt', '.md', '.cmake', '.lproject'}
        return Path(file_path).suffix.lower() in text_extensions
    
    def process_file_content(self, content: str) -> str:
        """Replace template variables in file content."""
        for key, value in self.replacements.items():
            content = content.replace(key, value)
        return content
    
    def process_filename(self, filename: str) -> str:
        """Replace template variables in filename."""
        for key, value in self.replacements.items():
            filename = filename.replace(key, value)
        return filename
    
    def copy_template(self, template_path: str, destination: str) -> bool:
        """Copy template directory and process all files."""
        try:
            total_files = sum(1 for _ in Path(template_path).rglob('*') if _.is_file())
            processed_files = 0
            
            for root, dirs, files in os.walk(template_path):
                # Skip template.json in root
                if root == template_path and 'template.json' in files:
                    files.remove('template.json')
                
                # Calculate relative path from template
                rel_path = os.path.relpath(root, template_path)
                
                # Process directory name
                if rel_path != '.':
                    processed_rel_path = self.process_filename(rel_path)
                    dest_dir = os.path.join(destination, processed_rel_path)
                else:
                    dest_dir = destination
                
                # Create destination directory
                os.makedirs(dest_dir, exist_ok=True)
                
                # Process files
                for file in files:
                    processed_files += 1
                    src_file = os.path.join(root, file)
                    
                    # Process filename
                    dest_filename = self.process_filename(file)
                    dest_file = os.path.join(dest_dir, dest_filename)
                    
                    # Copy and process file
                    if self.should_process_file(src_file):
                        try:
                            with open(src_file, 'r', encoding='utf-8') as f:
                                content = f.read()
                            
                            processed_content = self.process_file_content(content)
                            
                            with open(dest_file, 'w', encoding='utf-8') as f:
                                f.write(processed_content)
                            
                            self.log(f"Processed: {dest_filename}", "info")
                        except UnicodeDecodeError:
                            # If file isn't text, just copy it
                            shutil.copy2(src_file, dest_file)
                            self.log(f"Copied binary: {dest_filename}", "info")
                    else:
                        shutil.copy2(src_file, dest_file)
                        self.log(f"Copied: {dest_filename}", "info")
            
            return True
            
        except Exception as e:
            self.log(f"Error copying template: {e}", "error")
            return False
    
    def copy_tools_directory(self):
        """Copy Tools directory from engine to project."""
        src = os.path.join(self.lumina_dir, "Tools")
        dest = os.path.join(self.project_path, "Tools")
        
        if not os.path.exists(src):
            self.log("Tools directory not found in engine", "warning")
            return False
        
        try:
            if os.path.exists(dest):
                shutil.rmtree(dest)
            shutil.copytree(src, dest)
            self.log("Copied Tools directory", "success")
            return True
        except Exception as e:
            self.log(f"Error copying Tools: {e}", "error")
            return False
    
    def run_project_generation(self) -> bool:
        """Run the GenerateProject.py script if it exists."""
        generate_script = os.path.join(self.project_path, "GenerateProject.py")
        
        if not os.path.exists(generate_script):
            self.log("GenerateProject.py not found, skipping...", "warning")
            return True
        
        try:
            self.log("Running GenerateProject.py...", "info")
            result = subprocess.run(
                [sys.executable, generate_script],
                cwd=self.project_path,
                capture_output=True,
                text=True,
                timeout=60
            )
            
            if result.returncode == 0:
                self.log("Project generation completed!", "success")
                return True
            else:
                self.log(f"Project generation warning: {result.stderr}", "warning")
                return True
                
        except subprocess.TimeoutExpired:
            self.log("Project generation timed out", "warning")
            return True
        except Exception as e:
            self.log(f"Error running GenerateProject.py: {e}", "warning")
            return True


class LuminaProjectCreator:
    """Main application class for Lumina Project Creator."""
    
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Lumina Project Creator")
        self.root.geometry("700x600")
        self.root.resizable(False, False)
        
        # Apply dark theme
        self.setup_theme()
        
        # Variables
        self.project_path = tk.StringVar()
        self.project_name = tk.StringVar()
        self.selected_template = tk.StringVar()
        self.status_text = tk.StringVar(value="Ready to create a new project")
        
        # Get Lumina engine directory
        self.lumina_dir = os.getenv("LUMINA_DIR")
        
        # Initialize managers
        if self.lumina_dir:
            self.template_manager = TemplateManager(self.lumina_dir)
        else:
            self.template_manager = None
        
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
        
        style.configure("TCombobox",
            fieldbackground=DarkTheme.BG_MEDIUM,
            background=DarkTheme.BG_LIGHT,
            foreground=DarkTheme.BG_DARK,
            bordercolor=DarkTheme.BORDER,
            arrowcolor=DarkTheme.FG_PRIMARY,
            selectbackground=DarkTheme.ACCENT,
            selectforeground=DarkTheme.FG_PRIMARY
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
        name_entry.grid(row=0, column=1, sticky=(tk.W, tk.E), pady=5, padx=(0, 10), columnspan=2)
        name_entry.focus()
        
        # Template Selection
        ttk.Label(config_frame, text="Template:").grid(row=1, column=0, sticky=tk.W, pady=5, padx=(0, 10))
        self.template_combo = ttk.Combobox(
            config_frame, 
            textvariable=self.selected_template,
            state='readonly',
            font=("Segoe UI", 9)
        )
        self.template_combo.grid(row=1, column=1, sticky=(tk.W, tk.E), pady=5, padx=(0, 10), columnspan=2)
        self.template_combo.bind('<<ComboboxSelected>>', self.on_template_selected)
        
        # Template Description
        self.template_desc = ttk.Label(
            config_frame,
            text="Select a template to see description",
            foreground=DarkTheme.FG_SECONDARY,
            font=("Segoe UI", 8),
            wraplength=500
        )
        self.template_desc.grid(row=2, column=0, columnspan=3, sticky=tk.W, pady=(0, 5))
        
        # Project Directory
        ttk.Label(config_frame, text="Project Directory:").grid(row=3, column=0, sticky=tk.W, pady=5, padx=(0, 10))
        path_entry = ttk.Entry(config_frame, textvariable=self.project_path, font=("Segoe UI", 9))
        path_entry.grid(row=3, column=1, sticky=(tk.W, tk.E), pady=5, padx=(0, 10))
        
        browse_btn = ttk.Button(config_frame, text="Browse...", command=self.browse_directory, width=12)
        browse_btn.grid(row=3, column=2, pady=5)
        
        # Options Section
        options_frame = ttk.LabelFrame(main_frame, text="Additional Options", padding="10")
        options_frame.grid(row=3, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 15))
        
        self.starter_content_var = tk.BooleanVar()
        self.starter_content_check = ttk.Checkbutton(
            options_frame,
            text="Include Starter Content (Coming Soon)",
            variable=self.starter_content_var,
            state='disabled'
        )
        self.starter_content_check.grid(row=0, column=0, sticky=tk.W)
        
        # Progress Section
        progress_frame = ttk.Frame(main_frame)
        progress_frame.grid(row=4, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 15))
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
        button_frame.grid(row=5, column=0, columnspan=3, pady=(10, 0))
        
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
        console_frame.grid(row=6, column=0, columnspan=3, sticky=(tk.W, tk.E, tk.N, tk.S), pady=(15, 0))
        console_frame.columnconfigure(0, weight=1)
        console_frame.rowconfigure(0, weight=1)
        main_frame.rowconfigure(6, weight=1)
        
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
            self.load_templates()
        else:
            self.engine_label.config(text="Not found! Set LUMINA_DIR environment variable", foreground=DarkTheme.ERROR)
            self.log("Engine directory not found. Please set LUMINA_DIR environment variable.", "error")
            self.create_btn.config(state='disabled')
    
    def load_templates(self):
        """Load available templates."""
        if not self.template_manager:
            return
        
        templates = self.template_manager.get_available_templates()
        if templates:
            self.template_combo['values'] = templates
            self.template_combo.current(0)
            self.on_template_selected(None)
            self.log(f"Found {len(templates)} template(s)", "success")
        else:
            self.log("No templates found in Templates/Projects directory", "warning")
            self.create_btn.config(state='disabled')
    
    def on_template_selected(self, event):
        """Handle template selection."""
        template_name = self.selected_template.get()
        if template_name and self.template_manager:
            info = self.template_manager.get_template_info(template_name)
            description = info.get('description', 'No description available')
            self.template_desc.config(text=description)
    
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
        """Sanitize filename by replacing spaces and invalid characters."""
        return filename.replace(" ", "_").strip()
    
    def validate_inputs(self) -> bool:
        """Validate user inputs."""
        if not self.project_name.get().strip():
            messagebox.showerror("Invalid Input", "Please enter a project name.")
            return False
        
        if not self.project_path.get().strip():
            messagebox.showerror("Invalid Input", "Please select a project directory.")
            return False
        
        if not self.selected_template.get():
            messagebox.showerror("Invalid Input", "Please select a project template.")
            return False
        
        if not self.lumina_dir or not os.path.exists(self.lumina_dir):
            messagebox.showerror("Engine Not Found", "Lumina engine directory not found. Please set LUMINA_DIR environment variable.")
            return False
        
        return True
    
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
            template_name = self.selected_template.get()
            
            # Create project folder inside the selected directory
            project_folder = os.path.join(base_folder, name)
            
            # Check if project folder already exists
            if os.path.exists(project_folder):
                messagebox.showerror("Error", f"Project folder '{name}' already exists at this location!")
                self.create_btn.config(state='normal')
                return
            
            self.log(f"Creating project '{name}' using template '{template_name}'...", "info")
            self.update_progress(10, "Initializing...")
            
            # Get template path
            template_path = self.template_manager.get_template_path(template_name)
            if not template_path:
                raise Exception(f"Template '{template_name}' not found")
            
            # Create project directory
            os.makedirs(project_folder, exist_ok=True)
            self.log(f"Created project directory: {project_folder}", "success")
            
            # Initialize project creator
            creator = ProjectCreator(self.lumina_dir, self.log)
            creator.set_project_info(name, project_folder)
            
            # Copy and process template
            self.update_progress(30, "Copying template files...")
            if not creator.copy_template(template_path, project_folder):
                raise Exception("Failed to copy template")
            
            # Copy Tools directory
            self.update_progress(70, "Copying Tools directory...")
            creator.copy_tools_directory()
            
            # Run project generation
            self.update_progress(90, "Running project generation...")
            creator.run_project_generation()
            
            self.update_progress(100, "Project created successfully!")
            self.log(f"Project '{name}' created successfully!", "success")
            
            messagebox.showinfo("Success", f"Project '{name}' has been created successfully!\n\nLocation: {project_folder}")
            
            # Open project folder
            if sys.platform == 'win32':
                os.startfile(project_folder)
            elif sys.platform == 'darwin':
                subprocess.run(['open', project_folder])
            else:
                subprocess.run(['xdg-open', project_folder])
            
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