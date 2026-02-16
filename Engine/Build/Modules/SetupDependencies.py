import os
import shutil
from pathlib import Path
from Core import Logger
from Core.Utils import download_file, extract_zip
import BuildConfig as Config

def validate() -> bool:
    """Validate and install all dependencies."""
    Logger.subheader("Third-Party Dependencies")
    
    if not Config.DEPENDENCIES:
        Logger.info("No dependencies configured")
        return True
    
    all_ok = True
    for dep in Config.DEPENDENCIES:
        if not _check_dependency(dep):
            all_ok = False
    
    return all_ok

def _check_dependency(dep: Config.Dependency) -> bool:
    """Check if a dependency is installed, install if missing."""
    install_path = Path(dep.install_path)
    
    if install_path.exists():
        Logger.success(f"{dep.name} ({dep.version}) found")
        return True
    
    Logger.info(f"Installing {dep.name} ({dep.version})...")
    return _install_dependency(dep)

def _install_dependency(dep: Config.Dependency) -> bool:
    """Download and install a dependency."""
    zip_path = f"{dep.install_path}.zip"
    
    if not download_file(dep.url, zip_path):
        Logger.error(f"Failed to download {dep.name}")
        return False
    
    if not extract_zip(zip_path):
        Logger.error(f"Failed to extract {dep.name}")
        return False
    
    # Move from extracted folder to install path if needed
    if dep.extracted_folder:
        extracted = Path(dep.install_path).parent / dep.extracted_folder
        if extracted.exists():
            shutil.move(str(extracted), dep.install_path)
        else:
            # List what was actually extracted to help debug
            parent_dir = Path(dep.install_path).parent
            extracted_items = [d.name for d in parent_dir.iterdir() if d.is_dir() and d.name != Path(dep.install_path).name]
            Logger.warning(f"Expected extracted folder '{dep.extracted_folder}' not found. Found: {extracted_items}")
            # Try to find the extracted folder automatically
            for item in extracted_items:
                if dep.name.lower() in item.lower():
                    Logger.info(f"Found possible match: {item}, moving to {dep.install_path}")
                    shutil.move(str(parent_dir / item), dep.install_path)
                    break
    
    # Verify installation was successful
    if not Path(dep.install_path).exists():
        Logger.error(f"Installation failed: {dep.install_path} does not exist")
        return False
    
    Logger.success(f"{dep.name} installed")
    return True