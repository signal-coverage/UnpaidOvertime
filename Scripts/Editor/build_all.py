"""Entry point for the UNPAID: OVERTIME environment art pipeline.

Run headless (editor may stay open; only /Game/UO/ packages are written):
    UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<abs>/Scripts/Editor/build_all.py" -unattended -nosplash -nullrhi
Stages can be limited with the env var UO_ART_STAGES (comma list of textures,materials,meshes,audio,level).
The audio stage synthesizes and imports every sound (uo_audio_synth.py + uo_audio.py) and places the level audio.
"""
import importlib
import os
import sys
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import unreal  # noqa: E402

import uo_common  # noqa: E402
import uo_textures  # noqa: E402
import uo_materials  # noqa: E402
import uo_meshes  # noqa: E402
import uo_layout  # noqa: E402
import uo_level  # noqa: E402
import uo_audio_synth  # noqa: E402
import uo_audio  # noqa: E402

for _m in (uo_common, uo_textures, uo_materials, uo_meshes, uo_layout, uo_audio_synth, uo_audio, uo_level):
    importlib.reload(_m)

ALL_STAGES = ["textures", "materials", "meshes", "audio", "level"]


def main():
    stages = os.environ.get("UO_ART_STAGES", "").strip()
    stages = [s.strip().strip('"') for s in stages.replace(",", " ").split() if s.strip().strip('"')] or ALL_STAGES
    uo_common.log("stages: %s" % stages)
    t0 = time.time()
    ok = True
    for stage in stages:
        t = time.time()
        try:
            if stage == "textures":
                uo_textures.build()
            elif stage == "materials":
                uo_materials.build()
            elif stage == "meshes":
                only = os.environ.get("UO_ART_MESHES", "").strip()
                uo_meshes.build([s for s in only.split(",") if s] or None)
            elif stage == "audio":
                # A level rebuild later in this run places the level audio itself
                uo_audio.build(place_in_level="level" not in stages)
            elif stage == "level":
                uo_level.build()
            else:
                uo_common.warn("unknown stage " + stage)
        except Exception:
            ok = False
            uo_common.warn("stage %s failed:\n%s" % (stage, traceback.format_exc()))
            unreal.log_error("[UOArt] stage %s failed" % stage)
            break
        uo_common.log("stage %s took %.1fs" % (stage, time.time() - t))
    uo_common.log("%s in %.1fs" % ("DONE" if ok else "FAILED", time.time() - t0))


main()
