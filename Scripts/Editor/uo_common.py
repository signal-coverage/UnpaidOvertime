"""Shared helpers for the UNPAID: OVERTIME art pipeline (run inside Unreal's Python)."""
import os

import unreal

ROOT = "/Game/UO"
TEX_DIR = ROOT + "/Textures"
MAT_DIR = ROOT + "/Materials"
MESH_DIR = ROOT + "/Meshes"
MAP_DIR = ROOT + "/Maps"
MAP_PATH = MAP_DIR + "/Lvl_Office"

PROJECT_DIR = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
TEX_SOURCE_DIR = os.path.join(PROJECT_DIR, "Content", "UO", "Textures", "Source")
SCREENSHOT_DIR = os.path.join(PROJECT_DIR, "Saved", "Screenshots", "ArtPass")


LOG_FILE = os.path.join(PROJECT_DIR, "Saved", "Logs", "UOArt.log")


def _append(line):
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(line + "\n")
    except OSError:
        pass


def log(msg):
    unreal.log("[UOArt] " + str(msg))
    _append(str(msg))


def warn(msg):
    unreal.log_warning("[UOArt] " + str(msg))
    _append("WARNING " + str(msg))


def ensure_dir(path):
    if not os.path.isdir(path):
        os.makedirs(path)


def asset_exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def load(path):
    if path.startswith("/Engine/"):
        obj_path = path if "." in path.rsplit("/", 1)[-1] else path + "." + path.rsplit("/", 1)[-1]
        return unreal.load_object(None, obj_path)
    return unreal.EditorAssetLibrary.load_asset(path)


def save(asset_or_path):
    if isinstance(asset_or_path, str):
        unreal.EditorAssetLibrary.save_asset(asset_or_path, only_if_is_dirty=False)
    else:
        unreal.EditorAssetLibrary.save_loaded_asset(asset_or_path, only_if_is_dirty=False)


def asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def get_or_create(path, asset_class, factory):
    """Return the existing asset at path (if of the right class) or create a new one."""
    if asset_exists(path):
        existing = load(path)
        if existing and isinstance(existing, asset_class):
            return existing, False
        unreal.EditorAssetLibrary.delete_asset(path)
    folder, name = path.rsplit("/", 1)
    return asset_tools().create_asset(name, folder, asset_class, factory), True


def lc(r, g, b, a=1.0):
    return unreal.LinearColor(r, g, b, a)


def srgb(hex_or_tuple, a=1.0):
    """sRGB color (hex 'rrggbb' or 0-255 tuple) -> LinearColor."""
    if isinstance(hex_or_tuple, str):
        h = hex_or_tuple.lstrip("#")
        rgb = tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))
    else:
        rgb = hex_or_tuple

    def to_lin(c):
        c = c / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    return unreal.LinearColor(to_lin(rgb[0]), to_lin(rgb[1]), to_lin(rgb[2]), a)


def vec(x, y, z):
    return unreal.Vector(float(x), float(y), float(z))


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    return unreal.Rotator(roll=float(roll), pitch=float(pitch), yaw=float(yaw))


def xform(loc=(0, 0, 0), rotation=(0, 0, 0), scale=(1, 1, 1)):
    """rotation = (pitch, yaw, roll) in degrees."""
    return unreal.Transform(vec(*loc), rot(*rotation), vec(*scale))
