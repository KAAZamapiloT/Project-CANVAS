import unreal
import json
import os

# --- 1. Define where to find assets ---
TEXTURE_PATH = "/Game/DATABASE/textures/"
MATERIAL_PATH = "/Game/DATABASE/materials/"
MANIFEST_FILE_PATH = unreal.Paths.project_content_dir() + "asset_manifest.json"

# Manually define your gameplay tags
# The script can't know this, so you must maintain this list
PROMPTABLE_TAGS = [
    "Background.Wall",
    "Ground.Floor",
    "Player.Armor",
    "Environment.Sky"
]

def get_asset_names_from_path(path):
    """Gets all asset names from a given content folder path."""
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    
    # Use get_assets_by_path to find all assets
    asset_data_list = asset_registry.get_assets_by_path(path, recursive=True)
    
    asset_names = []
    for asset_data in asset_data_list:
        asset_names.append(asset_data.asset_name)
    
    return asset_names

def generate_manifest():
    print(f"Starting asset manifest generation...")
    
    # 2. Scan the asset registry
    texture_names = get_asset_names_from_path(TEXTURE_PATH)
    material_names = get_asset_names_from_path(MATERIAL_PATH)
    
    # 3. Build the final manifest dictionary
    manifest = {
        "textures": texture_names,
        "materials": material_names,
        "promptableTags": PROMPTABLE_TAGS
    }
    
    # 4. Write the file
    try:
        with open(MANIFEST_FILE_PATH, "w") as f:
            json.dump(manifest, f, indent=2)
        print(f"SUCCESS: Wrote asset_manifest.json to {MANIFEST_FILE_PATH}")
        print(f"Found {len(texture_names)} textures and {len(material_names)} materials.")
        
    except Exception as e:
        print(f"ERROR: Failed to write manifest file: {e}")

# --- Run the function ---
generate_manifest()