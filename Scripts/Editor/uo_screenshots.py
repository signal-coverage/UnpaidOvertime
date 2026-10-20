"""Renders verification screenshots of Lvl_Office from fixed cameras.

Runs inside a full (rendering) editor session:
    UnrealEditor.exe <uproject> /Game/UO/Maps/Lvl_Office -ExecutePythonScript="<abs>/Scripts/Editor/uo_screenshots.py"
Shots go to Saved/Screenshots/ArtPass/<name>.png, then the editor quits.
Env: UO_SHOTS (comma list of camera names to limit), UO_SHOT_WARMUP (seconds, default 75).
"""
import os
import shutil
import time

import unreal

PROJECT_DIR = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT_DIR = os.path.join(PROJECT_DIR, "Saved", "Screenshots", "ArtPass")
RES = (1600, 900)

# name: (location, (pitch, yaw))
CAMERAS = [
    ("01_Lobby", (560, 120, 165), (-4, 100)),
    ("02_OpenPlan_Overview", (880, 1650, 210), (-12, -32)),
    ("03_OpenPlan_Windows", (1900, 1450, 165), (-6, -90)),
    ("04_Cubicle_Closeup", (1530, 900, 150), (-24, -135)),
    ("05_Kitchen", (2620, 2120, 170), (-8, 32)),
    ("06_Kitchen_Counter", (3000, 2420, 160), (-14, 75)),
    ("07_CopyRoom", (3060, 1260, 165), (-8, 20)),
    ("08_Manager", (3040, 120, 170), (-8, 30)),
    ("09_Corridor", (120, 1925, 165), (-4, 0)),
    ("10_Records_CoffeeB", (1520, 2080, 165), (-8, 50)),
    ("11_Exterior", (2200, -2300, 380), (-6, 92)),
    ("12_Desk_Detail", (1450, 690, 140), (-22, 178)),
    ("13_Kitchen_Vending", (2850, 2420, 165), (-8, 185)),
]

LOG = os.path.join(PROJECT_DIR, "Saved", "Logs", "UOArt.log")


def log(msg):
    unreal.log("[UOShots] " + msg)
    try:
        with open(LOG, "a", encoding="utf-8") as f:
            f.write("[shots] " + msg + "\n")
    except OSError:
        pass


only = [s for s in os.environ.get("UO_SHOTS", "").split(",") if s]
cams = [c for c in CAMERAS if not only or c[0] in only]
warmup = float(os.environ.get("UO_SHOT_WARMUP", "75"))
state = {"phase": "warmup", "t": time.time(), "i": 0, "start": time.time(), "handle": None}
shot_dir = os.path.join(PROJECT_DIR, "Saved", "Screenshots")
if not os.path.isdir(OUT_DIR):
    os.makedirs(OUT_DIR)


def newest_png(after):
    best, best_t = None, after
    for root, _, files in os.walk(shot_dir):
        if os.path.abspath(root).startswith(os.path.abspath(OUT_DIR)):
            continue
        for fn in files:
            if fn.lower().endswith(".png"):
                p = os.path.join(root, fn)
                t = os.path.getmtime(p)
                if t > best_t:
                    best, best_t = p, t
    return best


def set_camera(loc, pr):
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    ues.set_level_viewport_camera_info(unreal.Vector(*loc), unreal.Rotator(roll=0, pitch=pr[0], yaw=pr[1]))
    try:
        les.editor_set_game_view(True)
        les.editor_set_viewport_realtime(True)
    except Exception:
        pass


def tick(dt):
    now = time.time()
    ph = state["phase"]
    if ph == "warmup":
        if now - state["t"] > warmup:
            log("warmup done, %d cameras" % len(cams))
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            unreal.SystemLibrary.execute_console_command(world, "r.ScreenPercentage 100")
            state["phase"] = "move"
    elif ph == "move":
        if state["i"] >= len(cams):
            state["phase"] = "done"
            return
        name, loc, pr = cams[state["i"]]
        set_camera(loc, pr)
        state["t"] = now
        state["phase"] = "settle"
    elif ph == "settle":
        if now - state["t"] > 7.0:
            name = cams[state["i"]][0]
            state["shot_t"] = time.time()
            # AutomationLibrary.take_high_res_screenshot overflows the stack when called from a Slate tick,
            # so use the HighResShot console command on the active level viewport instead.
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            unreal.SystemLibrary.execute_console_command(world, "HighResShot %dx%d" % RES)
            state["t"] = now
            state["phase"] = "collect"
    elif ph == "collect":
        if now - state["t"] > 4.0:
            name = cams[state["i"]][0]
            src = newest_png(state["shot_t"] - 1)
            if src:
                dst = os.path.join(OUT_DIR, name + ".png")
                shutil.copyfile(src, dst)
                log("shot %s <- %s" % (dst, src))
            elif now - state["t"] < 20:
                return
            else:
                log("shot %s missing" % name)
            state["i"] += 1
            state["phase"] = "move"
    elif ph == "done":
        log("all shots done in %.0fs" % (now - state["start"]))
        unreal.unregister_slate_post_tick_callback(state["handle"])
        try:
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        except Exception:
            pass
        unreal.SystemLibrary.quit_editor()


try:  # -ExecutePythonScript quits after the script returns unless we ask to stay alive
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
except Exception as exc:
    log("keep alive failed: %s" % exc)
state["handle"] = unreal.register_slate_post_tick_callback(tick)
log("screenshot script armed (warmup %.0fs)" % warmup)
