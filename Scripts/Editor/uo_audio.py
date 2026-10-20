"""Audio stage of the UNPAID: OVERTIME pipeline (runs inside Unreal's Python).

1. Synthesizes every WAV (uo_audio_synth.py -> Content/UO/Audio/Source/, plus the loudness report).
2. Imports them as SoundWaves under /Game/UO/Audio/<Group>/SW_*, sets looping, volume, sound class, attenuation.
3. Creates the mix assets: SoundClasses (Master > Music/SFX/Ambience/UI/Voice), SM_UOVolumes, attenuation presets,
   per-room reverb effects, and DA_UOSoundSet (the id -> waves table the game loads from DefaultGame.ini).
4. Places the level audio in Lvl_Office: ambient emitters (tagged UOAudio.*) and one AudioVolume with reverb per room.
   uo_level.build() calls place_level_audio() too, so a full level rebuild keeps the audio.

Idempotent: re-running reimports the waves over the same assets and replaces the level audio actors.
"""
import math
import os

import unreal

import uo_common as C
import uo_layout as L
import uo_audio_synth as SYN

AUDIO_DIR = C.ROOT + "/Audio"
MIX_DIR = AUDIO_DIR + "/Mix"
SOUNDSET_PATH = AUDIO_DIR + "/DA_UOSoundSet"
SOUND_SET_CLASS = "/Script/UnpaidOvertime.UOSoundSet"

AUDIO_TAG = "UOAudio"
LEVEL_FOLDER = "Audio"

# id prefix -> content folder
GROUPS = [
    (("Footstep.", "Player."), "Player"),
    (("Interact.", "Printer.", "Coffee.", "Drawer.", "Paper.", "Supply.", "Item.", "Task."), "Interaction"),
    (("Day.", "Eval.", "Boss.", "Vote."), "DayFlow"),
    (("UI.",), "UI"),
    (("Role.", "Sabotage.", "Terminal.", "Meeting.", "Kill."), "Killer"),
    (("Amb.",), "Ambience"),
    (("Music.",), "Music"),
]

# Playback per sound id (wave volume, attenuation preset) lives in uo_audio_synth.WAVE_SETTINGS (pure data,
# so the standalone loudness report can show in-game levels too).
WAVE_SETTINGS = SYN.WAVE_SETTINGS

# Attenuation presets: inner radius, falloff distance, distance model, LPF at max distance (Hz or None),
# occlusion, non-spatialized radius (start, end) or None
ATTENUATION = {
    "ATT_Footstep": dict(inner=150, falloff=1800, model="NATURAL_SOUND", lpf=4000, occlusion=True),
    "ATT_Interaction": dict(inner=150, falloff=2200, model="NATURAL_SOUND", lpf=4000, occlusion=True),
    "ATT_Hum": dict(inner=120, falloff=600, model="LINEAR", lpf=None, occlusion=False),
    "ATT_EmitterSmall": dict(inner=80, falloff=700, model="LOGARITHMIC", lpf=5000, occlusion=True),
    "ATT_Clock": dict(inner=50, falloff=520, model="LOGARITHMIC", lpf=None, occlusion=True),
    "ATT_EmitterRoom": dict(inner=500, falloff=1300, model="LINEAR", lpf=2500, occlusion=True, nonspatial=(600, 300)),
    "ATT_Creak": dict(inner=250, falloff=3500, model="NATURAL_SOUND", lpf=2500, occlusion=False),
    "ATT_Radio": dict(inner=120, falloff=950, model="LOGARITHMIC", lpf=3000, occlusion=True),
}

CATEGORY_CLASS = {"sfx": "SC_SFX", "ui": "SC_UI", "ambience": "SC_Ambience", "music": "SC_Music"}
CHILD_CLASSES = ["SC_Music", "SC_SFX", "SC_Ambience", "SC_UI", "SC_Voice"]
UI_CLASSES = ("SC_Music", "SC_UI")  # keep playing while the game is paused

# Reverb per room: decay (s), high-frequency gain, decay HF ratio, reflections gain, overall gain
REVERBS = {
    "RE_OpenPlan": dict(decay=0.75, gain_hf=0.45, decay_hf=0.5, refl=0.18, gain=0.3),     # carpet + ceiling tiles: damped
    "RE_Lobby": dict(decay=1.5, gain_hf=0.85, decay_hf=0.85, refl=0.35, gain=0.34),       # tile, big volume
    "RE_Kitchen": dict(decay=1.15, gain_hf=0.95, decay_hf=0.9, refl=0.45, gain=0.36),     # tile + hard counters: bright
    "RE_Corridor": dict(decay=1.9, gain_hf=0.7, decay_hf=0.7, refl=0.3, gain=0.34),       # long and narrow
    "RE_SmallOffice": dict(decay=0.55, gain_hf=0.55, decay_hf=0.55, refl=0.2, gain=0.28),
    "RE_Supply": dict(decay=0.45, gain_hf=0.9, decay_hf=0.85, refl=0.4, gain=0.3),
}
ROOM_REVERB = {
    "Lobby": "RE_Lobby", "OpenPlan": "RE_OpenPlan", "Manager": "RE_SmallOffice", "CopyRoom": "RE_SmallOffice",
    "Corridor": "RE_Corridor", "Supply": "RE_Supply", "Records": "RE_SmallOffice", "Kitchen": "RE_Kitchen",
}

_failures = []


def _set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as exc:
        _failures.append("%s.%s: %s" % (type(obj).__name__, prop, exc))
        return False


def group_for(sound_id):
    for prefixes, folder in GROUPS:
        if sound_id.startswith(prefixes):
            return folder
    return "Misc"


# ----------------------------------------------------------------------------------------------
# Mix assets
# ----------------------------------------------------------------------------------------------

def build_sound_classes():
    classes = {}
    for name in ["SC_Master"] + CHILD_CLASSES:
        sc, _ = C.get_or_create(MIX_DIR + "/" + name, unreal.SoundClass, unreal.SoundClassFactory())
        classes[name] = sc
    master = classes["SC_Master"]
    _set(master, "child_classes", [classes[n] for n in CHILD_CLASSES])
    for name in CHILD_CLASSES:
        sc = classes[name]
        _set(sc, "parent_class", master)
        props = sc.get_editor_property("properties")
        _set(props, "is_ui_sound", name in UI_CLASSES)
        _set(props, "is_music", name == "SC_Music")
        _set(sc, "properties", props)
    for sc in classes.values():
        C.save(sc)
    return classes


def build_mix():
    mix, _ = C.get_or_create(MIX_DIR + "/SM_UOVolumes", unreal.SoundMix, unreal.SoundMixFactory())
    C.save(mix)
    return mix


def build_attenuations():
    out = {}
    for name, p in ATTENUATION.items():
        att, _ = C.get_or_create(MIX_DIR + "/" + name, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
        s = att.get_editor_property("attenuation")
        _set(s, "attenuate", True)
        _set(s, "spatialize", True)
        _set(s, "attenuation_shape", unreal.AttenuationShape.SPHERE)
        _set(s, "attenuation_shape_extents", C.vec(p["inner"], 0, 0))
        _set(s, "falloff_distance", float(p["falloff"]))
        _set(s, "distance_algorithm", getattr(unreal.AttenuationDistanceModel, p["model"]))
        if p["lpf"]:
            _set(s, "attenuate_with_lpf", True)
            _set(s, "lpf_radius_min", float(p["inner"]))
            _set(s, "lpf_radius_max", float(p["inner"] + p["falloff"]))
            _set(s, "lpf_frequency_at_min", 20000.0)
            _set(s, "lpf_frequency_at_max", float(p["lpf"]))
        else:
            _set(s, "attenuate_with_lpf", False)
        _set(s, "enable_occlusion", bool(p["occlusion"]))
        if p["occlusion"]:
            _set(s, "occlusion_trace_channel", unreal.CollisionChannel.ECC_VISIBILITY)
            _set(s, "occlusion_low_pass_filter_frequency", 1400.0)
            _set(s, "occlusion_volume_attenuation", 0.5)
            _set(s, "occlusion_interpolation_time", 0.2)
        ns = p.get("nonspatial")
        if ns:
            _set(s, "non_spatialized_radius_start", float(ns[0]))
            _set(s, "non_spatialized_radius_end", float(ns[1]))
        _set(s, "enable_reverb_send", True)
        _set(att, "attenuation", s)
        C.save(att)
        out[name] = att
    return out


def build_reverbs():
    out = {}
    for name, p in REVERBS.items():
        rev, _ = C.get_or_create(MIX_DIR + "/" + name, unreal.ReverbEffect, unreal.ReverbEffectFactory())
        vals = {
            "density": 0.85, "diffusion": 0.9, "gain": p["gain"], "gain_hf": p["gain_hf"], "decay_time": p["decay"],
            "decay_hf_ratio": p["decay_hf"], "reflections_gain": p["refl"], "reflections_delay": 0.008,
            "late_gain": 1.1, "late_delay": 0.012 if p["decay"] < 1.6 else 0.03, "air_absorption_gain_hf": 0.994,
        }
        for k, v in vals.items():
            _set(rev, k, float(v))
        C.save(rev)
        out[name] = rev
    return out


# ----------------------------------------------------------------------------------------------
# Waves
# ----------------------------------------------------------------------------------------------

def import_waves(manifest, classes, attenuations):
    tasks = []
    for e in manifest:
        folder = AUDIO_DIR + "/" + group_for(e["id"])
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(SYN.OUT_DIR, e["file"]))
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("destination_name", e["asset"])
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tasks.append(task)
    C.asset_tools().import_asset_tasks(tasks)

    waves = {}
    missing = 0
    for e in manifest:
        path = "%s/%s/%s" % (AUDIO_DIR, group_for(e["id"]), e["asset"])
        wave = C.load(path) if C.asset_exists(path) else None
        if wave is None:
            missing += 1
            C.warn("import failed: " + path)
            continue
        volume, att_name = WAVE_SETTINGS.get(e["id"], (0.5, None))
        _set(wave, "looping", bool(e["loop"]))
        _set(wave, "volume", float(volume))
        _set(wave, "sound_class_object", classes[CATEGORY_CLASS[e["category"]]])
        _set(wave, "attenuation_settings", attenuations.get(att_name) if att_name else None)
        if e["loop"]:
            # Out-of-range loops keep their place silently instead of taking a voice
            _set(wave, "virtualization_mode", unreal.VirtualizationMode.PLAY_WHEN_SILENT)
        C.save(wave)
        waves.setdefault(e["id"], []).append(wave)
    C.log("audio: imported %d waves (%d ids, %d missing)" % (sum(len(v) for v in waves.values()), len(waves), missing))
    return waves


def build_sound_set(waves, classes, mix):
    cls = unreal.load_class(None, SOUND_SET_CLASS)
    if cls is None:
        C.warn("UOSoundSet class not found (build the UnpaidOvertimeEditor target first); sound set not created")
        return None
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    asset, _ = C.get_or_create(SOUNDSET_PATH, unreal.UOSoundSet, factory)
    entries = []
    for sound_id in sorted(waves):
        entry = unreal.UOSoundEntry()
        entry.set_editor_property("id", unreal.Name(sound_id))
        entry.set_editor_property("variants", waves[sound_id])
        entries.append(entry)
    _set(asset, "entries", entries)
    _set(asset, "volume_mix", mix)
    _set(asset, "master_class", classes["SC_Master"])
    _set(asset, "music_class", classes["SC_Music"])
    _set(asset, "sfx_class", classes["SC_SFX"])
    _set(asset, "ambience_class", classes["SC_Ambience"])
    _set(asset, "ui_class", classes["SC_UI"])
    _set(asset, "voice_class", classes["SC_Voice"])
    C.save(asset)
    C.log("audio: sound set %s with %d entries" % (SOUNDSET_PATH, len(entries)))
    return asset


# ----------------------------------------------------------------------------------------------
# Level audio
# ----------------------------------------------------------------------------------------------

def _eas():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _wave(sound_id):
    folder = group_for(sound_id)
    path = "%s/%s/SW_%s" % (AUDIO_DIR, folder, sound_id.replace(".", "_"))
    return C.load(path) if C.asset_exists(path) else None


def _emitter(label, sound_id, loc, kind, volume=1.0, auto=True, attenuation=None, folder=None):
    wave = _wave(sound_id)
    if wave is None:
        C.warn("audio: missing wave for %s" % sound_id)
        return None
    actor = _eas().spawn_actor_from_class(unreal.AmbientSound, C.vec(*loc), C.rot())
    actor.set_actor_label(label)
    actor.set_folder_path(LEVEL_FOLDER + "/" + (folder or kind))
    actor.set_editor_property("tags", [unreal.Name(AUDIO_TAG), unreal.Name(AUDIO_TAG + "." + kind)])
    comp = actor.get_component_by_class(unreal.AudioComponent)
    _set(comp, "sound", wave)
    _set(comp, "auto_activate", auto)
    _set(comp, "volume_multiplier", float(volume))
    if attenuation is not None:
        _set(comp, "attenuation_settings", attenuation)
    return actor


def _furniture_positions():
    return {name: (x, y, z, yaw) for _mesh, name, x, y, z, yaw, _extra in L.furniture()}


def _hum_positions():
    """One hum emitter per cluster of ceiling panels (~9 m cells, never across rooms)."""
    clusters = {}
    for x, y in L.ceiling_panels():
        room = None
        for name, (x0, x1, y0, y1) in L.LIT_ROOMS:
            if x0 <= x <= x1 and y0 <= y <= y1:
                room = name
                break
        key = (room, int(x // 900), int(y // 900))
        clusters.setdefault(key, []).append((x, y))
    out = []
    for key in sorted(clusters, key=str):
        pts = clusters[key]
        out.append((sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts)))
    return out


def _window_emitters():
    out = []
    for wid, axis, fixed, start, end, outward, spans, extra in L.EXTERIOR_WALLS:
        k = 0
        for s0, s1 in spans:
            for p in L.window_positions(s0, s1):
                if k % 2 == 0:
                    inward = -outward * 60.0
                    out.append((wid, (p, fixed + inward, 170.0) if axis == "x" else (fixed + inward, p, 170.0)))
                k += 1
    return out


def _audio_volume(label, rect, reverb):
    x0, x1, y0, y1 = rect
    vol = _eas().spawn_actor_from_class(unreal.AudioVolume, C.vec((x0 + x1) / 2.0, (y0 + y1) / 2.0, 140.0), C.rot())
    vol.set_actor_label(label)
    vol.set_folder_path(LEVEL_FOLDER + "/Reverb")
    vol.set_editor_property("tags", [unreal.Name(AUDIO_TAG), unreal.Name(AUDIO_TAG + ".Reverb")])
    origin, ext = vol.get_actor_bounds(False)
    if ext.x < 1.0 or ext.y < 1.0:
        C.warn("audio volume %s has no brush (extent %s)" % (label, ext))
    vol.set_actor_scale3d(C.vec((x1 - x0) / max(1.0, 2 * ext.x), (y1 - y0) / max(1.0, 2 * ext.y), 290.0 / max(1.0, 2 * ext.z)))
    rs = unreal.ReverbSettings()
    _set(rs, "apply_reverb", True)
    _set(rs, "reverb_effect", reverb)
    _set(rs, "volume", 0.55)
    _set(rs, "fade_time", 0.6)
    _set(vol, "settings", rs)
    _set(vol, "priority", 1.0)
    return vol


def clear_level_audio():
    removed = 0
    for actor in _eas().get_all_level_actors():
        try:
            tags = [str(t) for t in actor.get_editor_property("tags")]
        except Exception:
            continue
        if AUDIO_TAG in tags:
            _eas().destroy_actor(actor)
            removed += 1
    return removed


def place_level_audio():
    """Spawns the audio actors into the currently open editor world (Lvl_Office). Caller saves the map."""
    if _wave("Amb.Hum") is None:
        C.warn("audio: waves not imported yet; run the audio stage (BuildArt.bat audio). Level audio skipped.")
        return {}
    removed = clear_level_audio()
    radio_att = C.load(MIX_DIR + "/ATT_Radio") if C.asset_exists(MIX_DIR + "/ATT_Radio") else None
    counts = {}

    def add(kind, n=1):
        counts[kind] = counts.get(kind, 0) + n

    for i, (x, y) in enumerate(_hum_positions()):
        if _emitter("Hum_%02d" % i, "Amb.Hum", (x, y, L.CEILING_Z - 8), "Hum"):
            add("Hum")
    for i, (x, y) in enumerate(L.ceiling_vents()):
        if _emitter("Vent_%02d" % i, "Amb.Vent", (x, y, L.CEILING_Z - 5), "Vent"):
            add("Vent")
    if _emitter("OfficeMurmur", "Amb.OfficeMurmur", (1900, 900, 160), "Murmur"):
        add("Murmur")
    for i, (wid, loc) in enumerate(_window_emitters()):
        if _emitter("Exterior_%s_%02d" % (wid, i), "Amb.Exterior", loc, "Exterior"):
            add("Exterior")

    furn = _furniture_positions()
    for name, (x, y, z, yaw) in sorted(furn.items()):
        a = math.radians(yaw)
        if name.endswith("_Clock"):
            if _emitter("ClockTick_" + name, "Amb.ClockTick", (x + math.cos(a) * 8, y + math.sin(a) * 8, z), "Clock"):
                add("Clock")
        elif name.startswith("Kitchen_Fridge"):
            if _emitter("Fridge_" + name, "Amb.Fridge", (x, y, 120), "Fridge", volume=0.8):
                add("Fridge")
        elif name == "Kitchen_VendingMachine":
            if _emitter("Vending", "Amb.Vending", (x, y, 130), "Vending"):
                add("Vending")
        elif name == "CoffeeMaker_Kitchen_01":
            # Break room radio: plays the muzak quietly during the shift (switched by UUOAudioSubsystem)
            if _emitter("BreakRoomRadio", "Music.Menu", (x - 110, y, 105), "Radio", volume=0.35, auto=False, attenuation=radio_att):
                add("Radio")

    volumes = 0
    reverbs = {name: C.load(MIX_DIR + "/" + name) for name in REVERBS}
    for name, rect, _mat in L.FLOORS:
        reverb_name = ROOM_REVERB.get(name)
        if reverb_name and reverbs.get(reverb_name):
            _audio_volume("Reverb_" + name, rect, reverbs[reverb_name])
            volumes += 1
    add("ReverbVolume", volumes)
    C.log("audio: level audio placed %s (replaced %d old actors)" % (sorted(counts.items()), removed))
    return counts


def place_in_saved_map():
    if not C.asset_exists(C.MAP_PATH):
        C.warn("audio: %s does not exist yet; run the level stage" % C.MAP_PATH)
        return
    world = unreal.EditorLoadingAndSavingUtils.load_map(C.MAP_PATH)
    if world is None:
        C.warn("audio: could not load %s" % C.MAP_PATH)
        return
    place_level_audio()
    saved = unreal.EditorLoadingAndSavingUtils.save_map(world, C.MAP_PATH)
    C.log("audio: map saved=%s" % saved)


# ----------------------------------------------------------------------------------------------
# Stage entry
# ----------------------------------------------------------------------------------------------

def build(place_in_level=True):
    del _failures[:]
    if os.environ.get("UO_AUDIO_SKIP_SYNTH", "") == "1" and os.path.isfile(os.path.join(SYN.OUT_DIR, "manifest.json")):
        import json
        with open(os.path.join(SYN.OUT_DIR, "manifest.json")) as f:
            manifest = json.load(f)["sounds"]
        C.log("audio: synthesis skipped (UO_AUDIO_SKIP_SYNTH=1)")
    else:
        manifest = SYN.generate()
    rows = SYN.report(manifest)
    flagged = [e["file"] for e, s in rows if s["verdict"] != "ok"]
    C.log("audio: %d files analysed, flagged: %s" % (len(rows), flagged or "none"))

    classes = build_sound_classes()
    mix = build_mix()
    attenuations = build_attenuations()
    build_reverbs()
    waves = import_waves(manifest, classes, attenuations)
    build_sound_set(waves, classes, mix)
    if place_in_level:
        place_in_saved_map()
    for msg in _failures:
        C.warn("audio property: " + msg)
    C.log("audio: %d property assignment failures" % len(_failures))
