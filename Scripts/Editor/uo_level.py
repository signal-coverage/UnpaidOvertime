"""Builds and saves /Game/UO/Maps/Lvl_Office (World Partition off) from uo_layout."""
import math

import unreal

import uo_common as C
import uo_layout as L
import uo_materials as M
import uo_meshes as MESH

# Lighting tuning (physical units). Interiors read bright/fluorescent; the sun is kept moderate so the
# parking lot stays readable through the windows instead of blowing out.
# Lighting pass: cooler, brighter panels and a higher exposure bias for the clean fluorescent-office look
# (the ceiling's own faint glow lives in uo_materials.CEILING_GLOW). Overtime dims all of this at runtime.
SUN_LUX = 6000.0
SUN_PITCH = -30.0
SUN_YAW = 128.0
PANEL_LUMENS = 4600.0
PANEL_KELVIN = 6100.0
EXPOSURE_MIN_EV = 7.6
EXPOSURE_MAX_EV = 9.4
EXPOSURE_BIAS = 1.05
SKY_INTENSITY = 1.4

# Main menu camera fly-through: consecutive pairs (from, to) of (location, (pitch, yaw))
MENU_CAMERA_SHOTS = [
    (((560, 140, 165), (-3, 98)), ((500, 620, 170), (-1, 94))),          # lobby, drifting toward the company sign
    (((880, 1650, 210), (-12, -32)), ((1260, 1610, 205), (-11, -44))),   # open plan overview
    (((2620, 2120, 170), (-8, 32)), ((2820, 2190, 168), (-8, 46))),      # break room
    (((3060, 1260, 165), (-8, 20)), ((3160, 1140, 166), (-7, 36))),      # copy room
]

STATION_TAG = "UOStation."
TERMINAL_TAG = "UOTerminal."
FURNITURE_TAG = "UOFurniture."
ROOM_TAG = "UORoom."
ROOM_VOLUME_TAG = "UORoomVolume"

# Boss question facts (counted at runtime from these tags, so they never drift from the level).
# mesh -> (UOProp kind, default color or None, shape or None)
PROP_KINDS = {
    "SM_Plant_Small": ("Plant", None, None),
    "SM_Plant_Large": ("Plant", None, None),
    "SM_Plant_Bush": ("Plant", None, None),
    "SM_Printer_Floor": ("Printer", None, None),
    "SM_Printer_Desktop": ("Printer", None, None),
    "SM_CoffeeBrewer": ("CoffeeMaker", "Silver", None),
    "SM_Fridge": ("Fridge", "Silver", None),
    "SM_Microwave": ("Microwave", None, None),
    "SM_FilingCabinet": ("FilingCabinet", None, None),
    "SM_OfficeChair": ("Chair", None, None),
    "SM_PlasticChair": ("Chair", None, None),
    "SM_PosterFrame": ("Poster", None, None),
    "SM_WallClock": ("Clock", None, "Square"),
    "SM_Mug": ("Mug", "Red", None),
    "SM_RecycleBin": ("RecycleBin", "Blue", None),
    "SM_WaterCooler": ("WaterCooler", "White", None),
    "SM_VendingMachine": ("VendingMachine", "Black", None),
    "SM_Sofa": ("Sofa", "Blue", None),
    "SM_Whiteboard": ("Whiteboard", None, None),
}

# Material override -> color name, for the slot that carries the prop's main color
COLOR_OVERRIDES = {
    "MI_Mug_Red": "Red", "MI_Mug_Yellow": "Yellow", "MI_Mug_Blue": "Blue", "MI_Mug_Green": "Green", "MI_Mug_White": "White",
    "MI_FabricRed": "Red", "MI_FabricBlue": "Blue", "MI_Fabric": "Gray",
    "MI_AccentGreen": "Green", "MI_AccentYellow": "Yellow", "MI_AccentRed": "Red", "MI_AccentBlue": "Blue",
}

# Rooms the boss knows about (the closed back rooms are not part of the game)
QUESTION_ROOMS = [name for name, _ in L.LIT_ROOMS]


def room_at(x, y):
    """Room id of a floor point (half-open rects from uo_layout.FLOORS), None outside the playable rooms."""
    for name, (x0, x1, y0, y1), _ in L.FLOORS:
        if name in QUESTION_ROOMS and x0 <= x < x1 and y0 <= y < y1:
            return name
    return None


def fact_tags(mesh, x, y, mats):
    """UORoom/UOProp/UOColor/UOText/UOShape tags of a furniture placement (empty outside rooms)."""
    room = room_at(x, y)
    if room is None:
        return []
    tags = [ROOM_TAG + room]
    kind = PROP_KINDS.get(mesh)
    if kind is None:
        return tags
    prop, color, shape = kind
    tags.append("UOProp." + prop)
    mats = mats or {}
    if prop == "Poster":
        poster = mats.get(1, "")
        if poster.startswith("MI_Poster_"):
            tags.append("UOText." + poster[len("MI_Poster_"):].upper())
    elif color is not None:
        override = mats.get(0) or mats.get(1)
        tags.append("UOColor." + COLOR_OVERRIDES.get(override, color))
    if shape:
        tags.append("UOShape." + shape)
    return tags

_eas = None
_mesh_cache = {}
_names = set()
_stats = {}


def eas():
    global _eas
    if _eas is None:
        _eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return _eas


def mesh_asset(name):
    if name in _mesh_cache:
        return _mesh_cache[name]
    found = None
    for folder in ("Office", "Kitchen", "Architecture", "Exterior", "Architecture/Walls", "Architecture/Floors", "Props"):
        p = "%s/%s/%s" % (C.MESH_DIR, folder, name)
        if C.asset_exists(p):
            found = C.load(p)
            break
    if found is None:
        C.warn("mesh not found: " + name)
    _mesh_cache[name] = found
    return found


def _unique(name):
    base = name
    i = 2
    while name in _names:
        name = "%s_%d" % (base, i)
        i += 1
    _names.add(name)
    return name


def _name_actor(actor, name, folder):
    name = _unique(name)
    try:
        actor.rename(name)
    except Exception as exc:
        C.warn("rename %s failed: %s" % (name, exc))
    actor.set_actor_label(name)
    if folder:
        actor.set_folder_path(folder)
    return name


def place_mesh(mesh_name, name, loc, yaw=0.0, folder=None, mats=None, pitch=0.0, roll=0.0, scale=None):
    sm = mesh_asset(mesh_name)
    if sm is None:
        return None
    # spawn_actor_from_object needs the editor UI's actor factories; spawning the class works headless.
    actor = eas().spawn_actor_from_class(unreal.StaticMeshActor, C.vec(*loc), C.rot(pitch, yaw, roll))
    if actor is None:
        C.warn("spawn failed " + mesh_name)
        return None
    _name_actor(actor, name, folder)
    if scale:
        actor.set_actor_scale3d(C.vec(*scale))
    comp = actor.static_mesh_component
    comp.set_static_mesh(sm)
    if mats:
        for idx, mat in mats.items():
            comp.set_material(int(idx), M.get(mat))
    _stats[mesh_name] = _stats.get(mesh_name, 0) + 1
    return actor


def spawn_class(cls, name, loc, rot=(0, 0, 0), folder=None):
    actor = eas().spawn_actor_from_class(cls, C.vec(*loc), C.rot(*rot))
    _name_actor(actor, name, folder)
    return actor


def target_point(name, loc, yaw, tags, folder):
    tp = spawn_class(unreal.TargetPoint, name, loc, (0, yaw, 0), folder)
    tp.set_editor_property("tags", [unreal.Name(t) for t in tags])
    return tp


# ----------------------------------------------------------------------------------------------
# Architecture
# ----------------------------------------------------------------------------------------------

def build_architecture():
    f = "Architecture"
    for wid, axis, fixed, start, end, openings, mat, kind in L.INTERIOR_WALLS:
        place_mesh("SM_Wall_" + wid, "Wall_" + wid, (0, 0, 0), folder=f + "/Walls")
        if kind == "glass":
            continue
        for o0, o1 in openings:
            mid = (o0 + o1) / 2.0
            w = int(round(o1 - o0))
            loc = (mid, fixed, 0) if axis == "x" else (fixed, mid, 0)
            place_mesh("SM_DoorFrame_%d" % w, "DoorFrame_%s_%d" % (wid, int(mid)), loc, 90 if axis == "x" else 0, f + "/Doors")
    for wid, axis, fixed, start, end, outward, spans, extra in L.EXTERIOR_WALLS:
        place_mesh("SM_ExtWall_" + wid, "ExtWall_" + wid, (0, 0, 0), folder=f + "/Walls")
        c = fixed + outward * L.EXT_T / 2.0
        inward = -outward
        if axis == "x":
            yaw = 90 if inward > 0 else -90
        else:
            yaw = 0 if inward > 0 else 180
        k = 0
        for s0, s1 in spans:
            for p in L.window_positions(s0, s1):
                loc = (p, c, 0) if axis == "x" else (c, p, 0)
                place_mesh("SM_WindowModule", "Window_%s_%02d" % (wid, k), loc, yaw, f + "/Windows")
                k += 1
        for o0, o1, kind in extra:
            mid = (o0 + o1) / 2.0
            loc = (mid, c, 0) if axis == "x" else (c, mid, 0)
            place_mesh("SM_EntranceDoors", "EntranceDoors_%s" % wid, loc, yaw, f + "/Doors")
    for name, rect, mat in L.FLOORS:
        place_mesh("SM_Floor_" + name, "Floor_" + name, (0, 0, 0), folder=f + "/Floors")
    place_mesh("SM_Ceiling", "Ceiling", (0, 0, 0), folder=f)
    place_mesh("SM_Roof", "Roof", (0, 0, 0), folder=f)
    place_mesh("SM_AccentPanels", "AccentPanels", (0, 0, 0), folder=f)
    place_mesh("SM_Ground", "Ground", (0, 0, 0), folder="Exterior")
    for did, x, y, yaw, sign, leaf in L.DECOR_DOORS:
        place_mesh("SM_DoorDecor", did, (x, y, 0), yaw, f + "/Doors", mats={0: leaf, 1: sign})


def build_ceiling_lights():
    f = "Lighting/CeilingPanels"
    panels = L.ceiling_panels()
    for i, (x, y) in enumerate(panels):
        place_mesh("SM_CeilingLightPanel", "LightPanel_%03d" % i, (x, y, L.CEILING_Z), 0, f)
        light = spawn_class(unreal.RectLight, "RectLight_%03d" % i, (x, y, L.CEILING_Z - 3), (-90, 0, 0), "Lighting/RectLights")
        comp = light.get_component_by_class(unreal.RectLightComponent)
        comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        comp.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
        comp.set_editor_property("intensity", PANEL_LUMENS)
        comp.set_editor_property("use_temperature", True)
        comp.set_editor_property("temperature", PANEL_KELVIN)
        comp.set_editor_property("source_width", 120.0)
        comp.set_editor_property("source_height", 60.0)
        comp.set_editor_property("barn_door_angle", 70.0)
        comp.set_editor_property("barn_door_length", 4.0)
        comp.set_editor_property("attenuation_radius", 900.0)
        comp.set_editor_property("cast_shadows", i % 3 == 0)
        comp.set_editor_property("specular_scale", 0.6)
    for i, (x, y) in enumerate(L.ceiling_vents()):
        place_mesh("SM_CeilingVent", "CeilingVent_%02d" % i, (x, y, L.CEILING_Z), 0, "Architecture/Ceiling")
    _stats["RectLight"] = len(panels)


def build_sky():
    f = "Lighting"
    sun = spawn_class(unreal.DirectionalLight, "Sun", (0, 0, 1500), (SUN_PITCH, SUN_YAW, 0), f)
    sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sc.set_editor_property("intensity", SUN_LUX)
    sc.set_editor_property("light_color", unreal.Color(r=255, g=244, b=228, a=255))
    sc.set_editor_property("atmosphere_sun_light", True)
    sc.set_editor_property("light_source_angle", 0.8)
    sky = spawn_class(unreal.SkyLight, "SkyLight", (0, 0, 1600), (0, 0, 0), f)
    kc = sky.get_component_by_class(unreal.SkyLightComponent)
    kc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    kc.set_editor_property("real_time_capture", True)
    kc.set_editor_property("intensity", SKY_INTENSITY)
    spawn_class(unreal.SkyAtmosphere, "SkyAtmosphere", (0, 0, 0), (0, 0, 0), f)
    fog = spawn_class(unreal.ExponentialHeightFog, "HeightFog", (0, 0, -100), (0, 0, 0), f)
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density", 0.004)
    fc.set_editor_property("fog_height_falloff", 0.15)
    fc.set_editor_property("start_distance", 3000.0)

    ppv = spawn_class(unreal.PostProcessVolume, "PostProcess_Global", (2000, 1400, 150), (0, 0, 0), f)
    ppv.set_editor_property("unbound", True)
    s = ppv.get_editor_property("settings")
    vals = {
        "auto_exposure_method": unreal.AutoExposureMethod.AEM_HISTOGRAM,
        "auto_exposure_min_brightness": EXPOSURE_MIN_EV,
        "auto_exposure_max_brightness": EXPOSURE_MAX_EV,
        "auto_exposure_bias": EXPOSURE_BIAS,
        "auto_exposure_speed_up": 3.0,
        "auto_exposure_speed_down": 2.0,
        "bloom_intensity": 0.25,
        "bloom_threshold": 1.5,
        "vignette_intensity": 0.32,
        "film_toe": 0.52,
        "film_shoulder": 0.3,
        "color_saturation": unreal.Vector4(1.0, 1.0, 1.0, 0.98),
        "lens_flare_intensity": 0.0,
        "ambient_occlusion_intensity": 0.55,
    }
    for k, v in vals.items():
        try:
            s.set_editor_property("override_" + k, True)
            s.set_editor_property(k, v)
        except Exception as exc:
            C.warn("ppv %s: %s" % (k, exc))
    ppv.set_editor_property("settings", s)


def build_furniture():
    anchors = {}
    facts = {}
    for mesh, name, x, y, z, yaw, extra in L.furniture() + L.exterior():
        folder = "Office/" + extra.get("folder", "Misc") if not extra.get("folder", "").startswith("Exterior") else extra["folder"]
        actor = place_mesh(mesh, name, (x, y, z), yaw, folder, extra.get("mats"))
        if actor is None:
            continue
        tags = fact_tags(mesh, x, y, extra.get("mats"))
        if tags:
            actor.set_editor_property("tags", [unreal.Name(t) for t in tags])
            props = [t for t in tags if t.startswith("UOProp.")]
            if props:
                key = "%s/%s" % (tags[0][len(ROOM_TAG):], props[0][len("UOProp."):])
                facts[key] = facts.get(key, 0) + 1
        station = extra.get("station")
        if station:
            sm = mesh_asset(mesh)
            ext = sm.get_bounds().box_extent
            dist = ext.x + 55.0
            if z > 10:
                dist = max(dist, 78.0)
            a = math.radians(yaw)
            ax, ay = x + math.cos(a) * dist, y + math.sin(a) * dist
            real = actor.get_name()
            tag = STATION_TAG + station
            target_point("Anchor_" + real, (ax, ay, 5), yaw + 180, [tag, FURNITURE_TAG + real], "Gameplay/Stations/" + station)
            anchors[tag] = anchors.get(tag, 0) + 1
        terminal = extra.get("terminal")
        if terminal:
            real = actor.get_name()
            room = room_at(x, y)
            if terminal in ("MovableProp", "SecurityCamera") and room is not None:
                # Plants and wall cameras: stand toward the middle of their room (their yaw says nothing useful)
                rect = dict(L.LIT_ROOMS)[room]
                cx, cy = (rect[0] + rect[1]) / 2.0, (rect[2] + rect[3]) / 2.0
                dx, dy = cx - x, cy - y
                d = max(1.0, math.hypot(dx, dy))
                dist = 95.0 if terminal == "MovableProp" else 120.0
                ax, ay = x + dx / d * dist, y + dy / d * dist
                face = math.degrees(math.atan2(-dy, -dx))
            else:
                sm = mesh_asset(mesh)
                ext = sm.get_bounds().box_extent
                dist = max(ext.x + 55.0, 78.0 if z > 10 else 0.0)
                if terminal in ("DeskComputer", "ActivityBoard"):
                    dist = 135.0  # monitors sit on desks/counters: stand clear of the furniture, behind the chair
                a = math.radians(yaw)
                ax, ay = x + math.cos(a) * dist, y + math.sin(a) * dist
                face = yaw + 180
            tags = [TERMINAL_TAG + terminal, FURNITURE_TAG + real]
            if room is not None:
                tags.append(ROOM_TAG + room)
            target_point("Terminal_" + real, (ax, ay, 5), face, tags, "Gameplay/Terminals/" + terminal)
            anchors[TERMINAL_TAG + terminal] = anchors.get(TERMINAL_TAG + terminal, 0) + 1
    C.log("boss facts: %s" % sorted(facts.items()))
    return anchors


def build_room_volumes():
    """One TriggerBox per playable room, tagged UORoomVolume + UORoom.<Id>: the game reads their 2D bounds to know
    who was in which room (boss question fairness). Collision is turned off at runtime."""
    f = "Gameplay/Rooms"
    for name, (x0, x1, y0, y1) in L.LIT_ROOMS:
        box = spawn_class(unreal.TriggerBox, "Room_" + name, ((x0 + x1) / 2.0, (y0 + y1) / 2.0, 150.0), (0, 0, 0), f)
        comp = box.get_component_by_class(unreal.BoxComponent)
        comp.set_editor_property("box_extent", C.vec((x1 - x0) / 2.0, (y1 - y0) / 2.0, 150.0))
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        box.set_editor_property("tags", [unreal.Name(ROOM_VOLUME_TAG), unreal.Name(ROOM_TAG + name)])
    C.log("room volumes: %d" % len(L.LIT_ROOMS))


def build_gameplay_points():
    f = "Gameplay"
    spawn_class(unreal.PlayerStart, "PlayerStart_Lobby", (560, 420, 100), (0, 90, 0), f)
    for i, (x, y) in enumerate([(470, 640), (620, 640), (470, 800), (620, 800)]):
        target_point("AISpawn_%d" % (i + 1), (x, y, 5), 90, ["UOSpawn.AI"], f + "/AISpawns")
    target_point("CompanySign_Anchor", (300, 1800 - L.INT_T / 2 - 4, 165), -90, ["UOSign.Company"], f)
    order = 0
    for shot in MENU_CAMERA_SHOTS:
        for (loc, (pitch, yaw)) in shot:
            tp = spawn_class(unreal.TargetPoint, "MenuCamera_%02d" % order, loc, (pitch, yaw, 0), f + "/MenuCamera")
            tp.set_editor_property("tags", [unreal.Name("UOCamera.Menu"), unreal.Name("UOOrder.%d" % order)])
            order += 1


def build_nav():
    x0, x1, y0, y1 = L.BUILDING
    nav = spawn_class(unreal.NavMeshBoundsVolume, "NavMeshBounds_Office", ((x0 + x1) / 2, (y0 + y1) / 2, 120), (0, 0, 0), "Gameplay")
    origin, ext = nav.get_actor_bounds(False)
    sx = (x1 - x0 + 100) / max(1.0, 2 * ext.x)
    sy = (y1 - y0 + 100) / max(1.0, 2 * ext.y)
    sz = 300.0 / max(1.0, 2 * ext.z)
    nav.set_actor_scale3d(C.vec(sx, sy, sz))
    origin, ext = nav.get_actor_bounds(False)
    C.log("nav bounds extent %s" % ext)


def build():
    global _eas
    _eas = None
    _mesh_cache.clear()
    _names.clear()
    _stats.clear()
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if C.asset_exists(C.MAP_PATH):
        unreal.EditorAssetLibrary.delete_asset(C.MAP_PATH)
    ok = False
    try:
        ok = les.new_level(C.MAP_PATH, False)
    except Exception as exc:
        C.warn("new_level failed: %s" % exc)
    if not ok:
        world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        unreal.EditorLoadingAndSavingUtils.save_map(world, C.MAP_PATH)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    C.log("level world: %s" % world.get_path_name())

    build_architecture()
    build_ceiling_lights()
    build_sky()
    anchors = build_furniture()
    build_room_volumes()
    build_gameplay_points()
    build_nav()
    try:
        import uo_audio
        uo_audio.place_level_audio()
    except Exception as exc:
        C.warn("level audio skipped: %s" % exc)

    ws = world.get_world_settings()
    gm = unreal.load_class(None, "/Script/UnpaidOvertime.UOGameMode")
    if gm:
        ws.set_editor_property("default_game_mode", gm)
    else:
        C.warn("UOGameMode class not found; GameMode override not set")
    try:
        C.log("world partition: %s" % ws.get_editor_property("world_partition"))
    except Exception:
        pass

    saved = unreal.EditorLoadingAndSavingUtils.save_map(world, C.MAP_PATH)
    C.log("level saved=%s actors=%d" % (saved, len(eas().get_all_level_actors())))
    for tag, n in sorted(anchors.items()):
        C.log("anchors %s = %d" % (tag, n))
    C.log("mesh instance counts: %s" % sorted(_stats.items()))
