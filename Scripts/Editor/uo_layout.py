"""Office floor plan for Lvl_Office (pure data, centimetres, Z up, X east, Y north).

Interior footprint: X 0..4000, Y 0..2800 (40 m x 28 m). Floor at Z=0, suspended ceiling at Z=270.

    y=2800 +-----------+-----+-----------+---------------------+
           | WC  WC SRV|SUPPL| RECORDS   |   BREAK ROOM        |  (north band, y 2050..2800)
    y=2050 +--d---d--d-+--d--+  (open)   +---ooo-----ooo-------+
           |                 CORRIDOR (y 1800..2050)            |
    y=1800 +----oo---+--ooo------ooo------+-------------------+
           |         |                    |    COPY ROOM      |
           |  LOBBY  o    OPEN PLAN       o                   |  (east windows)
           |         o   (3 cubicle pods) +-------------------+ y=900
           |         |                    g  MANAGER (glass)  |
    y=0    +--EE-----+--------------------+-------------------+
          x=0       800                 3000                4000
    (o = opening, d = decor door, g = glass wall, EE = entrance)

Furniture front convention: every furniture mesh faces +X (the side a person stands at / sits on).
A placement yaw rotates that front; the interaction anchor is placed in front of it, facing back.
"""

CEILING_Z = 270.0
WALL_H = 285.0          # interior walls go a bit into the plenum
EXT_WALL_H = 345.0      # exterior walls reach the roof slab
ROOF_Z = 345.0
INT_T = 12.0            # interior wall thickness
EXT_T = 30.0            # exterior wall thickness
DOOR_H = 220.0
WINDOW_PITCH = 300.0
WINDOW_W = 250.0
WINDOW_H = 262.0

BUILDING = (0.0, 4000.0, 0.0, 2800.0)

# Floor zones: name, (x0, x1, y0, y1), material
FLOORS = [
    ("Lobby", (0, 800, 0, 1800), "MI_LobbyTile"),
    ("OpenPlan", (800, 3000, 0, 1800), "MI_Carpet"),
    ("Manager", (3000, 4000, 0, 900), "MI_Carpet_Dark"),
    ("CopyRoom", (3000, 4000, 900, 1800), "MI_Carpet"),
    ("Corridor", (0, 4000, 1800, 2050), "MI_Carpet"),
    ("Supply", (1000, 1450, 2050, 2800), "MI_KitchenTile"),
    ("Records", (1450, 2500, 2050, 2800), "MI_Carpet"),
    ("Kitchen", (2500, 4000, 2050, 2800), "MI_KitchenTile"),
    ("Backrooms", (0, 1000, 2050, 2800), "MI_KitchenTile"),
]

# Ceiling coverage (rooms that get light panels). Backrooms (WC/server) are closed off.
LIT_ROOMS = [
    ("Lobby", (0, 800, 0, 1800)),
    ("OpenPlan", (800, 3000, 0, 1800)),
    ("Manager", (3000, 4000, 0, 900)),
    ("CopyRoom", (3000, 4000, 900, 1800)),
    ("Corridor", (0, 4000, 1800, 2050)),
    ("Supply", (1000, 1450, 2050, 2800)),
    ("Records", (1450, 2500, 2050, 2800)),
    ("Kitchen", (2500, 4000, 2050, 2800)),
]

# Interior walls: id, axis ('x' = runs along X at fixed y, 'y' = runs along Y at fixed x), fixed coord,
# start, end, openings [(start, end)], material, kind ('solid' or 'glass').
# Openings are walkable (>= 140 cm wide, 220 cm tall) and get a door frame casing.
INTERIOR_WALLS = [
    ("LobbyNorth", "x", 1800, 0, 800, [(500, 750)], "MI_WallPaint", "solid"),
    ("OpenPlanNorthA", "x", 1800, 800, 3000, [(1100, 1500), (2300, 2700)], "MI_WallPaint", "solid"),
    ("CopyNorth", "x", 1800, 3000, 4000, [], "MI_WallPaint", "solid"),
    ("LobbyEast", "y", 800, 0, 1800, [(450, 900)], "MI_WallWarm", "solid"),
    ("ManagerWest", "y", 3000, 0, 900, [(650, 800)], "MI_Glass", "glass"),
    ("ManagerNorth", "x", 900, 3000, 4000, [], "MI_WallPaint", "solid"),
    ("CopyWest", "y", 3000, 900, 1800, [(1200, 1360)], "MI_WallPaint", "solid"),
    ("CorridorNorthA", "x", 2050, 0, 1450, [(1150, 1300)], "MI_WallPaint", "solid"),
    ("CorridorNorthKitchen", "x", 2050, 2500, 4000, [(2850, 3150), (3500, 3700)], "MI_WallAccent_Green", "solid"),
    ("BackroomsEast", "y", 1000, 2050, 2800, [], "MI_WallPaint", "solid"),
    ("SupplyEast", "y", 1450, 2050, 2800, [], "MI_WallPaint", "solid"),
    ("KitchenWest", "y", 2500, 2050, 2800, [], "MI_WallPaint", "solid"),
]

# Exterior walls: id, axis, fixed coord (inner face), start, end, outward sign, window spans [(start, end)],
# extra openings [(start, end, kind)].
EXTERIOR_WALLS = [
    ("South", "x", 0, 0, 4000, -1, [(800, 3000), (3000, 4000), (0, 300), (550, 800)], [(300, 550, "entrance")]),
    ("East", "y", 4000, 0, 2800, +1, [(0, 900), (900, 1800), (2050, 2800)], []),
    ("North", "x", 2800, 0, 4000, +1, [], []),
    ("West", "y", 0, 0, 2800, -1, [], []),
]

# Decor (closed, non-walkable) doors on wall faces: id, x, y, yaw (front direction), sign material
DECOR_DOORS = [
    ("Door_Restroom_Men", 180, 2050 - INT_T / 2, -90, "MI_Sign_Men", "MI_DoorWood"),
    ("Door_Restroom_Women", 480, 2050 - INT_T / 2, -90, "MI_Sign_Women", "MI_DoorWood"),
    ("Door_ServerRoom", 820, 2050 - INT_T / 2, -90, "MI_Sign_Server", "MI_MetalDark"),
]


def window_positions(span_start, span_end):
    """Centres of window modules inside a span, pitch WINDOW_PITCH, leaving pillars at the ends."""
    length = span_end - span_start
    n = max(0, int((length + 1) // WINDOW_PITCH))
    if n == 0:
        return []
    used = n * WINDOW_PITCH
    first = span_start + (length - used) / 2.0 + WINDOW_PITCH / 2.0
    return [first + i * WINDOW_PITCH for i in range(n)]


def ceiling_panels():
    """(x, y) centres of 120x60 recessed LED panels on the 60 cm grid, inside lit rooms."""
    out = []
    for _, (x0, x1, y0, y1) in LIT_ROOMS:
        x = 120.0
        while x < 4000:
            y = 150.0
            while y < 2800:
                if x0 + 90 <= x <= x1 - 90 and y0 + 60 <= y <= y1 - 60:
                    out.append((x, y))
                y += 300.0
            x += 240.0
    # corridor is narrow: one centred row on the 60 grid
    out = [p for p in out if not (1800 <= p[1] <= 2050)]
    x = 120.0
    while x < 4000:
        out.append((x, 1950.0 - 30.0))
        x += 360.0
    return sorted(set(out))


def ceiling_vents():
    return [(1590, 1290), (2190, 270), (1050, 270), (2790, 1290), (3510, 450), (3510, 1350), (3210, 2430),
            (1950, 2430), (390, 990)]


# --------------------------------------------------------------------------------------------
# Furniture placements: (mesh, name, x, y, z, yaw, extras)
# extras: station tag, material overrides {slot_index: material}, folder
# --------------------------------------------------------------------------------------------

def _pod(cx, cy, idx):
    """Four L-desk workstations in a pinwheel around a cross of partitions. Returns placements."""
    out = []
    folder = "OpenPlan/Pod%d" % idx
    # cross partitions (two 170 cm panels per axis, meeting at the centre)
    out.append(("SM_Partition_170", "Pod%d_PartN" % idx, cx, cy + 87.5, 0, 0, {"folder": folder}))
    out.append(("SM_Partition_170", "Pod%d_PartS" % idx, cx, cy - 87.5, 0, 0, {"folder": folder}))
    out.append(("SM_Partition_170", "Pod%d_PartE" % idx, cx + 87.5, cy, 0, 90, {"folder": folder}))
    out.append(("SM_Partition_170", "Pod%d_PartW" % idx, cx - 87.5, cy, 0, 90, {"folder": folder}))
    out.append(("SM_PartitionPost", "Pod%d_PartPost" % idx, cx, cy, 0, 0, {"folder": folder}))
    # Quadrants of the pinwheel. In quadrant-local (u, v): u runs away from the partition at u=0 (desk back),
    # v runs away from the partition at v=0. Each quadrant is the previous one rotated 90 degrees.
    import math
    mug_mats = ["MI_Mug_Red", "MI_Mug_Yellow", "MI_Mug_Blue", "MI_Mug_Green"]
    for q, yaw in enumerate([0, 90, 180, 270]):
        n = "Pod%d_Desk%d" % (idx, q + 1)
        a = math.radians(yaw)
        ca, sa = math.cos(a), math.sin(a)

        def to_world(u, v):
            return cx + u * ca - v * sa, cy + u * sa + v * ca

        def put(mesh, name, u, v, z, dyaw=0.0, extra=None):
            x, y = to_world(u, v)
            d = {"folder": folder}
            d.update(extra or {})
            out.append((mesh, name, x, y, z, yaw + dyaw, d))

        # outer partition closes the cubicle along its far side (v = 172)
        x, y = to_world(65, 172)
        out.append(("SM_Partition_120", n + "_PartOuter", x, y, 0, yaw + 90, {"folder": folder}))
        put("SM_Desk_L", n, 78, 85, 0)
        put("SM_Pedestal", n + "_Pedestal", 37.5, 30, 0)
        put("SM_Monitor", n + "_Monitor", 26, 82, 74, extra={"terminal": "DeskComputer"} if q == (idx % 4) else None)
        put("SM_Keyboard", n + "_Keyboard", 52, 82, 74)
        put("SM_Mouse", n + "_Mouse", 54, 52, 74, 5)
        put("SM_OfficeChair", n + "_Chair", 118, 80, 0, 180 + ((q * 37) % 40) - 20)
        put("SM_DeskPhone", n + "_Phone", 100, 145, 74, -15)
        if q % 2 == 0:
            put("SM_PencilCup", n + "_PencilCup", 22, 125, 74)
            put("SM_Plant_Small", n + "_Succulent", 22, 150, 74, 30)
            put("SM_StickyPad", n + "_Sticky", 60, 125, 74, 12)
        else:
            put("SM_Mug", n + "_Mug", 60, 128, 74, 60, {"mats": {0: mug_mats[(idx + q) % 4]}})
            put("SM_Plant_Small", n + "_Succulent", 130, 150, 74, 10)
            put("SM_StickyPad", n + "_Sticky", 58, 36, 74, -8)
    return out


def furniture():
    P = []

    # ---------------- Lobby ----------------
    f = {"folder": "Lobby"}
    P.append(("SM_ReceptionCounter", "ReceptionCounter", 300, 1300, 0, -90, dict(f)))
    P.append(("SM_OfficeChair", "Reception_Chair", 300, 1420, 0, -90, dict(f)))
    P.append(("SM_Monitor", "Reception_Monitor", 250, 1340, 74, -90, dict(f, terminal="ActivityBoard")))
    P.append(("SM_Keyboard", "Reception_Keyboard", 250, 1370, 74, -90, dict(f)))
    P.append(("SM_InboxTray", "InboxTray_Reception", 400, 1268, 110, -90, dict(f, station="InboxTray")))
    P.append(("SM_Plant_Small", "Reception_Succulent", 180, 1268, 110, 0, dict(f)))
    P.append(("SM_CompanySignBoard", "CompanySign", 300, 1800 - INT_T / 2, 165, -90, dict(f)))
    P.append(("SM_Sofa", "Lobby_Sofa_A", 50, 420, 0, 0, dict(f)))
    P.append(("SM_Sofa", "Lobby_Sofa_B", 50, 900, 0, 0, dict(f, mats={0: "MI_FabricRed"})))
    P.append(("SM_CoffeeTableLow", "Lobby_CoffeeTable", 170, 660, 0, 0, dict(f)))
    P.append(("SM_Plant_Large", "Lobby_Plant_A", 50, 120, 0, 0, dict(f, terminal="MovableProp")))
    P.append(("SM_Plant_Bush", "Lobby_Plant_B", 60, 1180, 0, 0, dict(f)))
    P.append(("SM_Plant_Large", "Lobby_Plant_C", 740, 1730, 0, 45, dict(f)))
    P.append(("SM_WaterCooler", "Lobby_WaterCooler", 760, 1100, 0, 180, dict(f)))
    P.append(("SM_WallClock", "Lobby_Clock", 0 + 2, 660, 200, 0, dict(f)))
    P.append(("SM_PosterFrame", "Lobby_Poster_Synergy", 800 - INT_T / 2, 1300, 150, 180, dict(f, mats={1: "MI_Poster_Synergy"})))
    P.append(("SM_PosterFrame", "Lobby_Poster_Trust", 800 - INT_T / 2, 250, 150, 180, dict(f, mats={1: "MI_Poster_Trust"})))
    # Investigation: the badge reader next to the lobby -> open plan doorway, and a camera over reception
    P.append(("SM_BadgeReader", "Lobby_BadgeReader", 800 - INT_T / 2, 960, 115, 180, dict(f, terminal="BadgeReader")))
    P.append(("SM_SecurityCamera", "Lobby_Camera", 800 - INT_T / 2, 1720, 238, 180, dict(f, terminal="SecurityCamera")))

    # ---------------- Open plan ----------------
    for i, cx in enumerate([1300, 1900, 2500]):
        P.extend(_pod(cx, 640, i + 1))
    f = {"folder": "OpenPlan/North"}
    wall_y = 1800 - INT_T / 2
    for i, x in enumerate([1560, 1612, 1664, 1716]):
        P.append(("SM_FilingCabinet", "FilingCabinet_OpenPlan_%02d" % (i + 1), x, wall_y - 34, 0, -90, dict(f, station="FilingCabinet")))
    P.append(("SM_Printer_Floor", "Printer_Floor_OpenPlan_01", 1850, wall_y - 30, 0, -90, dict(f, station="Printer")))
    P.append(("SM_Printer_Floor", "Printer_Floor_OpenPlan_02", 1950, wall_y - 30, 0, -90, dict(f, station="Printer")))
    P.append(("SM_Credenza", "Credenza_OpenPlan", 2160, wall_y - 23, 0, -90, dict(f)))
    P.append(("SM_InboxTray", "InboxTray_OpenPlan_01", 2110, wall_y - 23, 75, -90, dict(f, station="InboxTray")))
    P.append(("SM_InboxTray", "InboxTray_OpenPlan_02", 2210, wall_y - 23, 75, -90, dict(f, station="InboxTray")))
    P.append(("SM_RecycleBin", "OpenPlan_Recycle_A", 2040, wall_y - 20, 0, -90, dict(f)))
    P.append(("SM_TrashBin", "OpenPlan_Trash_A", 2275, wall_y - 22, 0, -90, dict(f)))
    P.append(("SM_CoffeeCounter_240", "CoffeeCorner_A_Counter", 2850, wall_y - 31, 0, -90, dict(f)))
    P.append(("SM_CoffeeBrewer", "CoffeeMaker_CornerA_01", 2790, wall_y - 30, 90, -90, dict(f, station="CoffeeMaker")))
    P.append(("SM_CoffeeBrewer", "CoffeeMaker_CornerA_02", 2910, wall_y - 30, 90, -90, dict(f, station="CoffeeMaker")))
    P.append(("SM_Mug", "CornerA_Mug_1", 2740, wall_y - 45, 90, 10, dict(f, mats={0: "MI_Mug_White"})))
    P.append(("SM_Mug", "CornerA_Mug_2", 2955, wall_y - 50, 90, 200, dict(f, mats={0: "MI_Mug_Red"})))
    P.append(("SM_Plant_Large", "OpenPlan_Plant_N1", 860, wall_y - 40, 0, 0, dict(f)))
    P.append(("SM_WaterCooler", "OpenPlan_WaterCooler", 960, wall_y - 25, 0, -90, dict(f)))
    P.append(("SM_RecycleBin", "OpenPlan_Recycle_B", 1040, wall_y - 20, 0, -90, dict(f, mats={0: "MI_AccentGreen"})))
    P.append(("SM_PosterFrame", "OpenPlan_Poster_Overtime", 1640, wall_y, 175, -90, dict(f, mats={1: "MI_Poster_Overtime"})))
    P.append(("SM_PosterFrame", "OpenPlan_Poster_Teamwork", 2160, wall_y, 160, -90, dict(f, mats={1: "MI_Poster_Teamwork"})))
    P.append(("SM_WallClock", "OpenPlan_Clock", 1900, wall_y, 215, -90, dict(f)))
    P.append(("SM_SecurityCamera", "OpenPlan_Camera", 2480, wall_y, 238, -90, dict(f, terminal="SecurityCamera")))
    P.append(("SM_PosterFrame", "OpenPlan_Poster_Smile", 950, wall_y, 175, -90, dict(f, mats={1: "MI_Poster_Smile"})))
    # window-side plants and a team table
    f = {"folder": "OpenPlan/South"}
    for i, x in enumerate([1000, 1600, 2200, 2800]):
        P.append(("SM_Plant_Large" if i % 2 == 0 else "SM_Plant_Bush", "OpenPlan_Plant_S%d" % (i + 1), x, 45, 0, i * 40,
                  dict(f, terminal="MovableProp") if i in (0, 2) else dict(f)))
    # huddle tables between the pods and the north service wall
    f = {"folder": "OpenPlan/Huddle"}
    chair_mats = [None, {1: "MI_FabricRed"}, None, None, {1: "MI_FabricBlue"}, None]
    for t, tx in enumerate([1250, 2450]):
        ty = 1250
        P.append(("SM_MeetingTable", "OpenPlan_MeetingTable_%d" % (t + 1), tx, ty, 0, 90, dict(f)))
        k = 0
        for side, cyaw in ((1, -90), (-1, 90)):
            for dx in (-70, 0, 70):
                extra = dict(f)
                if chair_mats[k]:
                    extra["mats"] = chair_mats[k]
                P.append(("SM_OfficeChair", "OpenPlan_MeetingTable_%d_Chair_%d" % (t + 1, k + 1), tx + dx + (k * 13 % 9) - 4,
                          ty + side * 92, 0, cyaw + (k * 23 % 30) - 15, extra))
                k += 1
        P.append(("SM_Mug", "OpenPlan_MeetingTable_%d_Mug" % (t + 1), tx - 40, ty + 20, 74.5, 40 * t, dict(f, mats={0: "MI_Mug_White"})))
    P.append(("SM_Plant_Bush", "OpenPlan_Plant_Mid", 1900, 1180, 0, 0, dict(f)))
    P.append(("SM_Whiteboard", "OpenPlan_Whiteboard", 3000 - INT_T / 2, 1560, 150, 180, dict(f)))
    f = {"folder": "OpenPlan/South"}
    P.append(("SM_Desk_Straight", "OpenPlan_StandupDesk", 1600, 1250, 0, 90, dict(f)))
    P.append(("SM_Printer_Desktop", "Printer_Desktop_OpenPlan_Shared", 1560, 1245, 74, 90, dict(f)))
    P.append(("SM_Monitor", "OpenPlan_StandupDesk_Monitor", 1640, 1228, 74, 90, dict(f, terminal="DeskComputer")))

    # ---------------- Manager office ----------------
    f = {"folder": "Manager"}
    P.append(("SM_ManagerDesk", "Manager_Desk", 3560, 420, 0, 0, dict(f)))
    P.append(("SM_OfficeChair", "Manager_Chair", 3660, 420, 0, 180, dict(f, mats={1: "MI_FabricRed"})))
    P.append(("SM_PlasticChair", "Manager_VisitorChair_A", 3420, 360, 0, 0, dict(f, mats={0: "MI_PlasticBlack"})))
    P.append(("SM_PlasticChair", "Manager_VisitorChair_B", 3420, 480, 0, 0, dict(f, mats={0: "MI_PlasticBlack"})))
    P.append(("SM_Monitor", "Manager_Monitor", 3600, 440, 75, 180, dict(f)))
    P.append(("SM_Mug", "Manager_Mug", 3530, 350, 75, 30, dict(f, mats={0: "MI_Mug_Red"})))
    P.append(("SM_Bookshelf", "Manager_Bookshelf", 3420, 900 - INT_T / 2 - 18, 0, -90, dict(f)))
    P.append(("SM_FilingCabinet", "FilingCabinet_Manager", 3120, 900 - INT_T / 2 - 34, 0, -90, dict(f, station="FilingCabinet")))
    P.append(("SM_Credenza", "Credenza_Manager", 3760, 900 - INT_T / 2 - 23, 0, -90, dict(f)))
    P.append(("SM_SecurityMonitor", "Manager_SecurityMonitor", 3815, 900 - INT_T / 2 - 26, 75, -90, dict(f, terminal="SecurityMonitor")))
    P.append(("SM_Printer_Desktop", "Printer_Desktop_Manager", 3730, 900 - INT_T / 2 - 25, 75, -90, dict(f, station="Printer")))
    P.append(("SM_Plant_Bush", "Manager_Plant", 3930, 70, 0, 0, dict(f)))
    P.append(("SM_Rug", "Manager_Rug", 3540, 420, 0, 90, dict(f)))
    P.append(("SM_PosterFrame", "Manager_Poster_Overtime", 3880, 900 - INT_T / 2, 160, -90, dict(f, mats={1: "MI_Poster_Overtime"})))

    # ---------------- Copy room ----------------
    f = {"folder": "CopyRoom"}
    wall_y = 1800 - INT_T / 2
    P.append(("SM_Printer_Floor", "Printer_Floor_CopyRoom_01", 3250, wall_y - 30, 0, -90, dict(f, station="Printer")))
    P.append(("SM_Printer_Floor", "Printer_Floor_CopyRoom_02", 3400, wall_y - 30, 0, -90, dict(f, station="Printer")))
    P.append(("SM_SupplyShelf", "SupplyCloset_CopyRoom", 3700, wall_y - 23, 0, -90, dict(f, station="SupplyCloset")))
    counter_y = 900 + INT_T / 2 + 33
    P.append(("SM_CopyCounter_300", "CopyRoom_Counter", 3400, counter_y, 0, 90, dict(f)))
    P.append(("SM_Printer_Desktop", "Printer_Desktop_CopyRoom_01", 3290, counter_y - 8, 90, 90, dict(f, station="Printer")))
    P.append(("SM_Printer_Desktop", "Printer_Desktop_CopyRoom_02", 3510, counter_y - 8, 90, 90, dict(f, station="Printer")))
    P.append(("SM_InboxTray", "InboxTray_CopyRoom", 3400, counter_y - 10, 90, 90, dict(f, station="InboxTray")))
    P.append(("SM_RecycleBin", "CopyRoom_Recycle", 3080, 1700, 0, 0, dict(f)))
    P.append(("SM_TrashBin", "CopyRoom_Trash", 3080, 1620, 0, 0, dict(f)))
    P.append(("SM_MeetingTable", "CopyRoom_SortingTable", 3560, 1350, 0, 90, dict(f)))
    P.append(("SM_Printer_Floor", "CopyRoom_Printer_Extra", 3550, wall_y - 30, 0, -90, dict(f, terminal="PrintServer")))
    P.append(("SM_SecurityCamera", "CopyRoom_Camera", 3000 + INT_T / 2, 1080, 238, 0, dict(f, terminal="SecurityCamera")))
    P.append(("SM_InboxTray", "CopyRoom_Table_Tray", 3500, 1340, 74.5, 90, dict(f)))
    P.append(("SM_Plant_Large", "CopyRoom_Plant", 3940, 1740, 0, 0, dict(f, terminal="MovableProp")))
    P.append(("SM_PosterFrame", "CopyRoom_Poster", 3000 + INT_T / 2, 1600, 160, 0, dict(f, mats={1: "MI_Poster_Smile"})))

    # ---------------- Corridor ----------------
    f = {"folder": "Corridor"}
    P.append(("SM_WallClock", "Corridor_Clock", 1900, 1800 + INT_T / 2, 215, 90, dict(f)))
    P.append(("SM_PosterFrame", "Corridor_Poster_Teamwork", 2600, 1800 + INT_T / 2, 160, 90, dict(f, mats={1: "MI_Poster_Teamwork"})))
    P.append(("SM_PosterFrame", "Corridor_Poster_Synergy", 350, 1800 + INT_T / 2, 160, 90, dict(f, mats={1: "MI_Poster_Synergy"})))
    P.append(("SM_Plant_Large", "Corridor_Plant", 3950, 1930, 0, 0, dict(f, terminal="MovableProp")))
    P.append(("SM_SecurityCamera", "Corridor_Camera", 2000, 2050 - INT_T / 2, 238, -90, dict(f, terminal="SecurityCamera")))

    # ---------------- Supply room ----------------
    f = {"folder": "Supply"}
    P.append(("SM_SupplyShelf", "SupplyCloset_Main", 1225, 2800 - 23, 0, -90, dict(f, station="SupplyCloset")))
    P.append(("SM_SupplyShelf", "Supply_Shelf_West", 1000 + INT_T / 2 + 23, 2450, 0, 0, dict(f)))
    P.append(("SM_SupplyShelf", "Supply_Shelf_East", 1450 - INT_T / 2 - 23, 2450, 0, 180, dict(f)))
    P.append(("SM_InventoryClipboard", "Supply_InventorySheet", 1380, 2050 + INT_T / 2, 125, 90, dict(f, terminal="InventorySheet")))

    # ---------------- Records alcove ----------------
    f = {"folder": "Records"}
    for i, x in enumerate([1560, 1612, 1664]):
        P.append(("SM_FilingCabinet", "FilingCabinet_Records_%02d" % (i + 1), x, 2800 - 34, 0, -90, dict(f, station="FilingCabinet")))
    P.append(("SM_Printer_Floor", "Printer_Floor_Records", 1830, 2800 - 30, 0, -90, dict(f, station="Printer")))
    P.append(("SM_CoffeeCounter_240", "CoffeeCorner_B_Counter", 2300, 2800 - 31, 0, -90, dict(f)))
    P.append(("SM_CoffeeBrewer", "CoffeeMaker_CornerB_01", 2240, 2800 - 30, 90, -90, dict(f, station="CoffeeMaker")))
    P.append(("SM_CoffeeBrewer", "CoffeeMaker_CornerB_02", 2360, 2800 - 30, 90, -90, dict(f, station="CoffeeMaker")))
    P.append(("SM_Mug", "CornerB_Mug", 2420, 2800 - 40, 90, 75, dict(f, mats={0: "MI_Mug_Yellow"})))
    P.append(("SM_Plant_Large", "Records_Plant", 2455, 2110, 0, 0, dict(f, terminal="MovableProp")))
    P.append(("SM_FilingCabinet", "Records_Cabinet_Extra_01", 1716, 2800 - 34, 0, -90, dict(f)))
    P.append(("SM_Bookshelf", "Records_Bookshelf", 1990, 2800 - 18, 0, -90, dict(f)))
    P.append(("SM_Rug", "Records_Lounge_Rug", 1590, 2450, 0, 0, dict(f)))
    P.append(("SM_Sofa", "Records_Lounge_Sofa", 1456 + 46, 2450, 0, 0, dict(f, mats={0: "MI_Fabric"})))
    P.append(("SM_CoffeeTableLow", "Records_Lounge_Table", 1620, 2450, 0, 0, dict(f)))
    P.append(("SM_PlasticChair", "Records_Lounge_Chair", 1740, 2400, 0, 160, dict(f, mats={0: "MI_AccentGreen"})))
    P.append(("SM_MeetingTable", "Records_WorkTable", 2150, 2380, 0, 90, dict(f)))
    for k, (dx, side) in enumerate([(-60, 1), (60, 1), (-60, -1), (60, -1)]):
        P.append(("SM_PlasticChair", "Records_WorkTable_Chair_%d" % (k + 1), 2150 + dx, 2380 + side * 80, 0, -90 * side + (k * 11 % 13) - 6,
                  dict(f, mats={0: ["MI_PlasticWhite", "MI_AccentRed", "MI_PlasticBlack", "MI_AccentYellow"][k]})))
    P.append(("SM_Whiteboard", "Records_Whiteboard", 2500 - INT_T / 2, 2400, 150, 180, dict(f)))
    P.append(("SM_RecycleBin", "Records_Recycle", 2080, 2775, 0, -90, dict(f)))
    P.append(("SM_PosterFrame", "Records_Poster", 1450 + INT_T / 2, 2450, 160, 0, dict(f, mats={1: "MI_Poster_Trust"})))

    # ---------------- Break room / kitchen ----------------
    f = {"folder": "Kitchen"}
    wall_y = 2800
    for i in range(15):
        x = 2590 + i * 60
        P.append(("SM_Kitchen_Base60", "Kitchen_Base_%02d" % (i + 1), x, wall_y - 30, 0, -90, dict(f)))
    P.append(("SM_Countertop_Sink_900", "Kitchen_Countertop", 2590 + 7 * 60, wall_y - 31.5, 88, -90, dict(f)))
    P.append(("SM_Sink_Double", "Kitchen_Sink", 2590 + 7 * 60, wall_y - 33, 91.5, -90, dict(f)))
    P.append(("SM_DishRack", "Kitchen_DishRack", 2590 + 9 * 60, wall_y - 38, 91.5, -90, dict(f)))
    P.append(("SM_CoffeeBrewer", "CoffeeMaker_Kitchen_01", 3270, wall_y - 30, 91.5, -90, dict(f, station="CoffeeMaker")))
    P.append(("SM_CoffeeBrewer", "CoffeeMaker_Kitchen_02", 3390, wall_y - 30, 91.5, -90, dict(f, station="CoffeeMaker")))
    for i, m in enumerate(["MI_Mug_Red", "MI_Mug_Blue", "MI_Mug_Yellow", "MI_Mug_Green", "MI_Mug_White"]):
        P.append(("SM_Mug", "Kitchen_Mug_%d" % (i + 1), 3130 + i * 14, wall_y - 50 - (i % 2) * 8, 91.5, i * 50, dict(f, mats={0: m})))
    # uppers: cabinets with a two-microwave built-in niche
    for i in range(15):
        x = 2590 + i * 60
        if i in (2, 3):
            continue
        if 6 <= i <= 8:
            continue  # window-free zone over the sink stays open (shelf instead)
        P.append(("SM_Kitchen_Upper60", "Kitchen_Upper_%02d" % (i + 1), x, wall_y - 17.5, 145, -90, dict(f)))
    P.append(("SM_Kitchen_MicrowaveNiche120", "Kitchen_MicrowaveNiche", 2590 + 2.5 * 60, wall_y - 20, 145, -90, dict(f)))
    P.append(("SM_Microwave", "Kitchen_Microwave_01", 2590 + 2 * 60 + 2, wall_y - 18, 150, -90, dict(f)))
    P.append(("SM_Microwave", "Kitchen_Microwave_02", 2590 + 3 * 60 - 2, wall_y - 18, 150, -90, dict(f)))
    P.append(("SM_Kitchen_Shelf180", "Kitchen_OpenShelf", 2590 + 7 * 60, wall_y - 12, 165, -90, dict(f)))
    P.append(("SM_Fridge", "Kitchen_Fridge", 3545, wall_y - 38, 0, -90, dict(f)))
    P.append(("SM_Fridge", "Kitchen_Fridge_B", 3630, wall_y - 38, 0, -90, dict(f)))
    P.append(("SM_RecycleBin", "Kitchen_Recycle_Blue", 3720, wall_y - 20, 0, -90, dict(f)))
    P.append(("SM_RecycleBin", "Kitchen_Recycle_Green", 3765, wall_y - 20, 0, -90, dict(f, mats={0: "MI_AccentGreen"})))
    P.append(("SM_RecycleBin", "Kitchen_Recycle_Yellow", 3810, wall_y - 20, 0, -90, dict(f, mats={0: "MI_AccentYellow"})))
    P.append(("SM_TrashBin", "Kitchen_Trash", 3870, wall_y - 25, 0, -90, dict(f)))
    P.append(("SM_VendingMachine", "Kitchen_VendingMachine", 2500 + INT_T / 2 + 45, 2350, 0, 0, dict(f)))
    P.append(("SM_WaterCooler", "Kitchen_WaterCooler", 2500 + INT_T / 2 + 20, 2560, 0, 0, dict(f)))
    P.append(("SM_PosterFrame", "Kitchen_Poster_Smile", 2500 + INT_T / 2, 2680, 160, 0, dict(f, mats={1: "MI_Poster_Smile"})))
    P.append(("SM_WallClock", "Kitchen_Clock", 2500 + INT_T / 2, 2150, 210, 0, dict(f)))
    P.append(("SM_SecurityCamera", "Kitchen_Camera", 2500 + INT_T / 2, 2470, 238, 0, dict(f, terminal="SecurityCamera")))
    chair_mats = ["MI_AccentRed", "MI_PlasticWhite", "MI_AccentGreen", "MI_AccentYellow"]
    for t, (tx, ty) in enumerate([(2950, 2330), (3330, 2330), (3730, 2330)]):
        P.append(("SM_CafeTable", "Kitchen_Table_%d" % (t + 1), tx, ty, 0, 0, dict(f)))
        for c, ang in enumerate([0, 90, 180, 270]):
            import math
            a = math.radians(ang + 45 * (t % 2))
            P.append(("SM_PlasticChair", "Kitchen_Table_%d_Chair_%d" % (t + 1, c + 1), tx - math.cos(a) * 62, ty - math.sin(a) * 62, 0,
                      ang + 45 * (t % 2) + (7 * c % 11) - 5, dict(f, mats={0: chair_mats[(t + c) % 4]})))
        P.append(("SM_Mug", "Kitchen_Table_%d_Mug" % (t + 1), tx + 12, ty - 8, 74, t * 77, dict(f, mats={0: ["MI_Mug_Blue", "MI_Mug_Red", "MI_Mug_Green"][t]})))
    return P


# --------------------------------------------------------------------------------------------
# Exterior
# --------------------------------------------------------------------------------------------

def exterior():
    P = []
    f = {"folder": "Exterior/Cars"}
    car_mats = ["MI_CarPaint_Red", "MI_CarPaint_Blue", "MI_CarPaint_White", "MI_CarPaint_Silver", "MI_CarPaint_Green",
                "MI_CarPaint_Black", "MI_CarPaint_White", "MI_CarPaint_Silver"]
    import random
    rnd = random.Random(7)
    # south lot: two rows of stalls (stall width 260), cars nose-in
    k = 0
    for row, (y, yaw) in enumerate([(-1000, 90), (-2150, -90)]):
        for i in range(14):
            x = -300 + i * 260 + 130
            if rnd.random() < 0.62:
                P.append(("SM_Car_Sedan", "Car_South_%02d" % k, x + rnd.uniform(-12, 12), y + rnd.uniform(-20, 20), 0,
                          yaw + rnd.uniform(-3, 3), dict(f, mats={0: car_mats[k % len(car_mats)]})))
                k += 1
    # east lot
    for col, (x, yaw) in enumerate([(5250, 180), (6400, 0)]):
        for i in range(10):
            y = 100 + i * 260 + 130
            if rnd.random() < 0.55:
                P.append(("SM_Car_Sedan", "Car_East_%02d" % k, x + rnd.uniform(-20, 20), y + rnd.uniform(-12, 12), 0,
                          yaw + rnd.uniform(-3, 3), dict(f, mats={0: car_mats[k % len(car_mats)]})))
                k += 1
    f = {"folder": "Exterior/Trees"}
    t = 0
    for i in range(9):
        x = -200 + i * 500
        P.append(("SM_Tree_Round" if i % 3 else "SM_Tree_Cone", "Tree_South_%02d" % t, x + rnd.uniform(-60, 60), -560, 0, rnd.uniform(0, 360), dict(f)))
        t += 1
    for i in range(7):
        y = -100 + i * 480
        P.append(("SM_Tree_Round" if i % 2 else "SM_Tree_Cone", "Tree_East_%02d" % t, 4600 + rnd.uniform(-40, 40), y, 0, rnd.uniform(0, 360), dict(f)))
        t += 1
    for i in range(10):
        x = -800 + i * 900
        P.append(("SM_Tree_Round", "Tree_Far_%02d" % t, x + rnd.uniform(-100, 100), -3300 + rnd.uniform(-60, 60), 0, rnd.uniform(0, 360), dict(f)))
        t += 1
    for i in range(6):
        y = -1200 + i * 900
        P.append(("SM_Tree_Cone", "Tree_FarEast_%02d" % t, 7600 + rnd.uniform(-100, 100), y, 0, rnd.uniform(0, 360), dict(f)))
        t += 1
    f = {"folder": "Exterior/Lamps"}
    for i in range(5):
        P.append(("SM_LampPost", "LampPost_South_%d" % i, -100 + i * 900, -1575, 0, 90, dict(f)))
    for i in range(4):
        P.append(("SM_LampPost", "LampPost_East_%d" % i, 5825, 200 + i * 800, 0, 0, dict(f)))
    f = {"folder": "Exterior/Backdrop"}
    P.append(("SM_Building_A", "Backdrop_Building_South_A", 300, -5200, -15, 0, dict(f)))
    P.append(("SM_Building_B", "Backdrop_Building_South_B", 5200, -5000, -15, 0, dict(f, mats={0: "MI_FacadeAlt"})))
    P.append(("SM_Building_B", "Backdrop_Building_South_C", -4300, -4800, -15, 0, dict(f)))
    P.append(("SM_Building_A", "Backdrop_Building_East_A", 9400, 1200, -15, 90, dict(f, mats={0: "MI_FacadeAlt"})))
    P.append(("SM_Building_B", "Backdrop_Building_East_B", 9200, -2600, -15, 90, dict(f)))
    P.append(("SM_Building_B", "Backdrop_Building_East_C", 9300, 5200, -15, 90, dict(f, mats={0: "MI_FacadeAlt"})))
    for i in range(8):
        P.append(("SM_Hedge_400", "Hedge_South_%d" % i, -300 + i * 560, -420, 0, 0, dict(f)))
    for i in range(6):
        P.append(("SM_Hedge_400", "Hedge_East_%d" % i, 4440, -150 + i * 520, 0, 90, dict(f)))
    return P
