import subprocess
import sys

def EnsurePip():
    try:
        import pip
    except ImportError:
        print("pip not found. Installing pip...")
        subprocess.check_call([sys.executable, "-m", "ensurepip", "--upgrade"])

def InstallPackage(package):
    print(f"Installing {package} module...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "--upgrade", package])

# Make sure pip exists
EnsurePip()

# Mandatory packages
InstallPackage('setuptools')

import pkg_resources

def ValidatePackage(package):
    required = { package }
    installed = {pkg.key for pkg in pkg_resources.working_set}
    missing = required - installed

    if missing:
        InstallPackage(package)

def ValidatePackages():
    ValidatePackage('requests')
    ValidatePackage('fake-useragent')
    ValidatePackage('colorama')

ValidatePackages()
