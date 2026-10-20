"""Static meshes built with Geometry Script.

Conventions
- Units: centimetres, real-world scale. Pivot at the floor-centre of the footprint (Z=0 is the resting surface).
  Wall-hung items (posters, clocks, signs, decor doors) pivot at the centre of their back face.
- Front: every furniture mesh faces +X (the side a person stands at or sits on).
- Each part is bevelled individually (polygroup bevel) for soft, friendly edges, then box-projected UVs at
  1 UV = 100 cm in the part's own frame, so tiling textures keep real-world scale.
- Collision: explicit simple boxes (`col=True`) converted to aligned box shapes; otherwise one box = bounds.
- Architecture meshes are built in world space (actor at the origin) with world-aligned UVs so the ceiling
  grid, carpet tiles and floor tiles line up across rooms.
"""
import math
import random

import unreal

import uo_common as C
import uo_layout as L
import uo_materials as M

GS = unreal
PRIM = unreal.GeometryScript_Primitives
EDIT = unreal.GeometryScript_MeshEdits
MODEL = unreal.GeometryScript_MeshModeling
UVS = unreal.GeometryScript_UVs
NORM = unreal.GeometryScript_Normals
COLL = unreal.GeometryScript_Collision
NEW = unreal.GeometryScript_NewAssetUtils
XF = unreal.GeometryScript_MeshTransforms
QUERY = unreal.GeometryScript_MeshQueries
MATS = unreal.GeometryScript_MeshMaterials if hasattr(unreal, "GeometryScript_MeshMaterials") else unreal.GeometryScript_Materials

V2 = unreal.Vector2D


def _v2(x, y):
    return unreal.Vector2D(float(x), float(y))


def _xf(loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    return C.xform(loc, rot, scale)


IDENT = unreal.Transform()
_EMPTY_SEL = unreal.GeometryScriptMeshSelection()


class Builder:
    def __init__(self, name, folder="Props"):
        self.name = name
        self.folder = folder
        self.mesh = unreal.DynamicMesh()
        self.col = unreal.DynamicMesh()
        self.col_count = 0
        self.slots = []
        MATS.enable_material_i_ds(self.mesh)

    # ---- helpers ------------------------------------------------------------------------------
    def mid(self, mat):
        if mat not in self.slots:
            self.slots.append(mat)
        return self.slots.index(mat)

    def _opts(self, mat, fill_uv=False, group=unreal.GeometryScriptPrimitivePolygroupMode.PER_FACE):
        o = unreal.GeometryScriptPrimitiveOptions()
        o.set_editor_property("polygroup_mode", group)
        o.set_editor_property("material_id", self.mid(mat))
        self._part_mid = self.mid(mat)
        o.set_editor_property("uv_mode", unreal.GeometryScriptPrimitiveUVMode.SCALE_TO_FILL if fill_uv
                              else unreal.GeometryScriptPrimitiveUVMode.UNIFORM)
        return o

    @staticmethod
    def _fix_orientation(part):
        try:
            res = QUERY.get_mesh_volume_area(part)
            vol = res[1] if isinstance(res, (tuple, list)) else res
            if vol < 0:
                NORM.flip_normals(part)
        except Exception as exc:  # non-fatal
            C.warn("volume query failed: %s" % exc)

    def _commit(self, part, xf, bevel=0.0, uv="box", world_uv=False, mat=None):
        if bevel and bevel > 0.05:
            o = unreal.GeometryScriptMeshBevelOptions()
            o.set_editor_property("bevel_distance", float(bevel))
            o.set_editor_property("subdivisions", 2 if bevel >= 0.6 else 1)
            o.set_editor_property("round_weight", 1.0)
            o.set_editor_property("infer_material_id", True)
            MODEL.apply_mesh_polygroup_bevel(part, o)
        self._fix_orientation(part)
        # some generators (revolve, sweep, torus...) ignore PrimitiveOptions.MaterialID: force it
        MATS.enable_material_i_ds(part)
        if self._part_mid != 0:
            MATS.remap_material_i_ds(part, 0, self._part_mid)
        if world_uv:
            tmp = unreal.DynamicMesh()
            EDIT.append_mesh(tmp, part, xf)
            UVS.set_mesh_u_vs_from_box_projection(tmp, 0, _xf(scale=(100, 100, 100)), _EMPTY_SEL, 2)
            EDIT.append_mesh(self.mesh, tmp, IDENT)
            return
        if uv == "box":
            UVS.set_mesh_u_vs_from_box_projection(part, 0, _xf(scale=(100, 100, 100)), _EMPTY_SEL, 2)
        EDIT.append_mesh(self.mesh, part, xf)

    def colbox(self, x0, x1, y0, y1, z0, z1):
        PRIM.append_box(self.col, unreal.GeometryScriptPrimitiveOptions(),
                        _xf(((x0 + x1) / 2.0, (y0 + y1) / 2.0, z0)), float(abs(x1 - x0)), float(abs(y1 - y0)), float(abs(z1 - z0)))
        self.col_count += 1

    # ---- primitives -------------------------------------------------------------------------
    def box(self, mat, center, size, bevel=0.6, rot=(0, 0, 0), col=False, uv="box", fill_uv=False, world_uv=False):
        """center = (cx, cy, z0) with z0 = bottom of the box (before rotation)."""
        sx, sy, sz = [max(0.05, float(s)) for s in size]
        bevel = min(bevel, 0.45 * min(sx, sy, sz))
        part = unreal.DynamicMesh()
        PRIM.append_box(part, self._opts(mat, fill_uv), IDENT, sx, sy, sz)
        self._commit(part, _xf(center, rot), bevel, uv if not fill_uv else "keep", world_uv)
        if col:
            cx, cy, z0 = center
            self.colbox(cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2, z0, z0 + sz)

    def boxr(self, mat, x0, x1, y0, y1, z0, z1, bevel=0.6, col=False, world_uv=False, rot=(0, 0, 0)):
        self.box(mat, ((x0 + x1) / 2.0, (y0 + y1) / 2.0, z0), (x1 - x0, y1 - y0, z1 - z0), bevel, rot, col, world_uv=world_uv)

    def cyl(self, mat, center, radius, height, steps=16, bevel=0.0, rot=(0, 0, 0), col=False, uv="box", capped=True):
        part = unreal.DynamicMesh()
        PRIM.append_cylinder(part, self._opts(mat), IDENT, float(radius), float(height), int(steps), 0, capped)
        self._commit(part, _xf(center, rot), min(bevel, radius * 0.4, height * 0.4), uv)
        if col:
            cx, cy, z0 = center
            self.colbox(cx - radius, cx + radius, cy - radius, cy + radius, z0, z0 + height)

    def cone(self, mat, center, r0, r1, height, steps=16, rot=(0, 0, 0)):
        part = unreal.DynamicMesh()
        PRIM.append_cone(part, self._opts(mat), IDENT, float(r0), float(r1), float(height), int(steps), 1, True)
        self._commit(part, _xf(center, rot), 0.0)

    def sphere(self, mat, center, radius, scale=(1, 1, 1), steps=6, rot=(0, 0, 0)):
        part = unreal.DynamicMesh()
        PRIM.append_sphere_box(part, self._opts(mat, group=unreal.GeometryScriptPrimitivePolygroupMode.SINGLE_GROUP),
                               IDENT, float(radius), steps, steps, steps)
        XF.scale_mesh(part, C.vec(*scale), C.vec(0, 0, 0))
        self._commit(part, _xf(center, rot), 0.0)

    def revolve(self, mat, profile, center=(0, 0, 0), steps=24, rot=(0, 0, 0), capped=True, hard=False):
        """profile: list of (radius, z) from bottom to top."""
        part = unreal.DynamicMesh()
        ro = unreal.GeometryScriptRevolveOptions()
        ro.set_editor_property("hard_normals", hard)
        PRIM.append_revolve_path(part, self._opts(mat, group=unreal.GeometryScriptPrimitivePolygroupMode.SINGLE_GROUP),
                                 IDENT, [_v2(r, z) for r, z in profile], ro, int(steps), capped)
        self._commit(part, _xf(center, rot), 0.0)

    def torus(self, mat, center, major, minor, rot=(0, 0, 0), degrees=360.0, steps=(16, 8)):
        part = unreal.DynamicMesh()
        ro = unreal.GeometryScriptRevolveOptions()
        ro.set_editor_property("revolve_degrees", float(degrees))
        PRIM.append_torus(part, self._opts(mat, group=unreal.GeometryScriptPrimitivePolygroupMode.SINGLE_GROUP), IDENT, ro,
                          float(major), float(minor), steps[0], steps[1], unreal.GeometryScriptPrimitiveOriginMode.CENTER)
        self._commit(part, _xf(center, rot), 0.0)

    def sweep(self, mat, polygon, path, center=(0, 0, 0), rot=(0, 0, 0), end_scale=1.0, capped=True, bevel=0.0):
        part = unreal.DynamicMesh()
        PRIM.append_simple_swept_polygon(part, self._opts(mat), IDENT, [_v2(u, v) for u, v in polygon],
                                         [C.vec(*p) for p in path], False, capped, 1.0, float(end_scale), 0.0, 1.0)
        self._commit(part, _xf(center, rot), bevel)

    def extrude(self, mat, polygon, z0, height, center=(0, 0, 0), rot=(0, 0, 0), bevel=0.0, col=False):
        """Polygon in local XY extruded along +Z from z0."""
        part = unreal.DynamicMesh()
        PRIM.append_simple_extrude_polygon(part, self._opts(mat), _xf((0, 0, z0)), [_v2(x, y) for x, y in polygon],
                                           float(height), 0, True)
        self._commit(part, _xf(center, rot), bevel)
        if col:
            xs = [p[0] for p in polygon]
            ys = [p[1] for p in polygon]
            self.colbox(min(xs) + center[0], max(xs) + center[0], min(ys) + center[1], max(ys) + center[1], z0 + center[2], z0 + height + center[2])

    def profile_x(self, mat, profile_xz, y0, y1, bevel=0.0):
        """Side profile (x, z) polygon extruded along Y from y0 to y1 (used for cars, wedges)."""
        part = unreal.DynamicMesh()
        PRIM.append_simple_extrude_polygon(part, self._opts(mat), IDENT, [_v2(x, z) for x, z in profile_xz],
                                           float(y1 - y0), 0, True)
        # polygon lives in XY, extrusion along Z: rotate so local Y -> world Z and local Z -> world Y
        tmp = unreal.DynamicMesh()
        m = unreal.Matrix
        EDIT.append_mesh(tmp, part, IDENT)
        # swap Y/Z via a 90 deg roll; sign checked with the volume test afterwards
        XF.transform_mesh(tmp, _xf((0, 0, 0), (0, 0, 90)))
        b = QUERY.get_mesh_bounding_box(tmp)
        lo = b.min
        hi = b.max
        # ensure the extrusion lands on [y0, y1] whatever the roll sign
        XF.translate_mesh(tmp, C.vec(0, y0 - lo.y, 0))
        if hi.z < 0.01:  # roll went the other way (polygon y -> -z): mirror back up
            XF.scale_mesh(tmp, C.vec(1, 1, -1), C.vec(0, 0, 0))
        self._commit(tmp, IDENT, bevel)

    # ---- finish -------------------------------------------------------------------------------
    def save(self, recenter=False):
        mesh = self.mesh
        if recenter:
            b = QUERY.get_mesh_bounding_box(mesh)
            off = C.vec(-(b.min.x + b.max.x) / 2.0, -(b.min.y + b.max.y) / 2.0, 0)
            XF.translate_mesh(mesh, off)
            if self.col_count:
                XF.translate_mesh(self.col, off)
        so = unreal.GeometryScriptSplitNormalsOptions()
        so.set_editor_property("split_by_opening_angle", True)
        so.set_editor_property("opening_angle_deg", 36.0)
        NORM.compute_split_normals(mesh, so, unreal.GeometryScriptCalculateNormalsOptions())
        if self.col_count == 0:
            b = QUERY.get_mesh_bounding_box(mesh)
            PRIM.append_box(self.col, unreal.GeometryScriptPrimitiveOptions(),
                            _xf(((b.min.x + b.max.x) / 2, (b.min.y + b.max.y) / 2, b.min.z)),
                            max(1.0, b.max.x - b.min.x), max(1.0, b.max.y - b.min.y), max(1.0, b.max.z - b.min.z))

        path = "%s/%s/%s" % (C.MESH_DIR, self.folder, self.name)
        mats = [M.get(s) for s in self.slots]
        sm = None
        if C.asset_exists(path):
            sm = C.load(path)
            opts = unreal.GeometryScriptCopyMeshToAssetOptions()
            opts.set_editor_property("enable_recompute_normals", False)
            opts.set_editor_property("enable_recompute_tangents", True)
            opts.set_editor_property("replace_materials", True)
            opts.set_editor_property("new_materials", mats)
            opts.set_editor_property("new_material_slot_names", [unreal.Name(s) for s in self.slots])
            lod = unreal.GeometryScriptMeshWriteLOD()
            res = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, sm, opts, lod)
        else:
            opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
            opts.set_editor_property("enable_recompute_normals", False)
            opts.set_editor_property("enable_recompute_tangents", True)
            opts.set_editor_property("enable_nanite", False)
            opts.set_editor_property("enable_collision", True)
            res = NEW.create_new_static_mesh_asset_from_mesh(mesh, path, opts)
            sm = res[0] if isinstance(res, (tuple, list)) else res
            if sm:
                sm.set_editor_property("static_materials",
                                       [unreal.StaticMaterial(material_interface=m, material_slot_name=unreal.Name(s))
                                        for m, s in zip(mats, self.slots)])
        if not sm:
            C.warn("failed to create mesh " + path)
            return None
        co = unreal.GeometryScriptCollisionFromMeshOptions()
        co.set_editor_property("method", unreal.GeometryScriptCollisionGenerationMethod.ALIGNED_BOXES)
        co.set_editor_property("auto_detect_spheres", False)
        co.set_editor_property("auto_detect_boxes", False)
        co.set_editor_property("auto_detect_capsules", False)
        co.set_editor_property("emit_transaction", False)
        co.set_editor_property("remove_fully_contained_shapes", True)
        COLL.set_static_mesh_collision_from_mesh(self.col, sm, co)
        C.save(sm)
        return sm


# =============================================================================================
# Office furniture
# =============================================================================================

def desk_legs_panel(b, x0, x1, y, z1, mat="MI_MetalGray"):
    b.boxr(mat, x0, x1, y - 1.5, y + 1.5, 0, z1, 0.6, col=True)
    b.boxr("MI_PlasticBlack", x0 + 2, x0 + 6, y - 2, y + 2, 0, 1.5, 0.3)
    b.boxr("MI_PlasticBlack", x1 - 6, x1 - 2, y - 2, y + 2, 0, 1.5, 0.3)


def mk_desk_straight():
    b = Builder("SM_Desk_Straight", "Office")
    b.boxr("MI_WoodTop", -37.5, 37.5, -75, 75, 71.5, 74, 0.9, col=True)
    for y in (-71, 71):
        desk_legs_panel(b, -34, 34, y, 71.5)
    b.boxr("MI_MetalGray", -34, -31, -70, 70, 28, 71.5, 0.6)
    b.cyl("MI_PlasticBlack", (-28, 55, 73.6), 3.0, 0.6, 16)
    return b.save()


def mk_desk_l():
    b = Builder("SM_Desk_L", "Office")
    poly = [(-75, -80), (-5, -80), (-5, 20), (75, 20), (75, 80), (-75, 80)]
    b.extrude("MI_WoodTop", poly, 71.5, 2.5, bevel=0.9)
    b.colbox(-75, -5, -80, 80, 0, 74)
    b.colbox(-5, 75, 20, 80, 0, 74)
    desk_legs_panel(b, -73, -7, -77, 71.5)
    # modesty panel along the back and under the return
    b.boxr("MI_MetalGray", -74, -71, -76, 76, 25, 71.5, 0.6)
    b.boxr("MI_MetalGray", -5, 73, 76, 79, 25, 71.5, 0.6)
    # end leg of the return
    b.boxr("MI_MetalGray", 70, 73, 22, 78, 0, 71.5, 0.6)
    b.boxr("MI_PlasticBlack", 69, 74, 24, 28, 0, 1.5, 0.3)
    b.boxr("MI_PlasticBlack", 69, 74, 72, 76, 0, 1.5, 0.3)
    # cable grommet
    b.cyl("MI_PlasticBlack", (-66, 60, 73.6), 3.2, 0.6, 16)
    return b.save()


def mk_pedestal():
    b = Builder("SM_Pedestal", "Office")
    b.boxr("MI_MetalGray", -27.5, 26, -20, 20, 3, 66, 1.0, col=True)
    b.boxr("MI_PlasticBlack", -25, 22, -18, 18, 0, 3, 0.4)
    fronts = [(46, 66), (26, 45.4), (4, 25.4)]
    for z0, z1 in fronts:
        b.boxr("MI_MetalGray", 25.6, 27.6, -19.4, 19.4, z0 + 0.4, z1 - 0.2, 0.6)
        b.boxr("MI_PlasticBlack", 27.4, 28.6, -8, 8, z1 - 5.5, z1 - 3.5, 0.4)
    b.cyl("MI_Chrome", (27.8, 14, 61.5), 0.9, 0.6, 10, rot=(0, 0, 90))
    return b.save()


def partition(name, length):
    b = Builder(name, "Office")
    h_fabric = 100.0
    half = length / 2.0
    b.boxr("MI_MetalDark", -3, 3, -half, half, 0, 5, 0.8, col=True)
    b.boxr("MI_Fabric", -2.5, 2.5, -half + 2.5, half - 2.5, 5, 5 + h_fabric, 0.8)
    b.boxr("MI_MetalDark", -2.8, 2.8, -half + 2, half - 2, 5 + h_fabric, 7 + h_fabric, 0.4)
    b.boxr("MI_GlassFrosted", -0.6, 0.6, -half + 2.5, half - 2.5, 7 + h_fabric, 7 + h_fabric + 18, 0.0)
    b.boxr("MI_MetalDark", -3, 3, -half, half, 125, 128, 1.2)
    for y in (-half + 1.5, half - 1.5):
        b.boxr("MI_MetalDark", -3, 3, y - 1.5, y + 1.5, 0, 128, 0.8)
    b.colbox(-3, 3, -half, half, 0, 128)
    return b.save()


def mk_partition_post():
    b = Builder("SM_PartitionPost", "Office")
    b.boxr("MI_MetalDark", -4, 4, -4, 4, 0, 129, 1.2, col=True)
    return b.save()


def mk_office_chair():
    b = Builder("SM_OfficeChair", "Office")
    # 5-star base with casters
    for k in range(5):
        a = k * 72.0
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        b.box("MI_PlasticBlack", (ca * 14, sa * 14, 7), (28, 5, 3.2), 1.2, rot=(4, a, 0))
        b.sphere("MI_PlasticBlack", (ca * 28.5, sa * 28.5, 3.2), 3.2, (1.0, 0.7, 1.0), 4, rot=(0, a + 90, 0))
        b.box("MI_PlasticGray", (ca * 28.5, sa * 28.5, 5.5), (2.0, 2.0, 3.0), 0.3)
    b.cyl("MI_PlasticBlack", (0, 0, 6), 5.0, 7.0, 16, 1.5)
    b.cyl("MI_Chrome", (0, 0, 12), 2.4, 30.0, 16)
    b.boxr("MI_PlasticBlack", -9, 9, -9, 9, 41, 46, 1.0)
    b.cyl("MI_PlasticBlack", (6, -10, 42.5), 0.8, 9, 8, rot=(0, 0, 90))
    # seat
    b.boxr("MI_PlasticBlack", -24, 24, -24, 24, 45, 48, 1.3)
    b.boxr("MI_ChairMesh", -23, 25, -23, 23, 47, 54.5, 3.4)
    # spine + backrest (tilted back ~10 degrees)
    b.box("MI_PlasticBlack", (-24, 0, 47), (4, 7, 22), 1.2, rot=(8, 0, 0))
    b.box("MI_PlasticBlack", (-29, 0, 62), (4.5, 46, 54), 2.0, rot=(-10, 0, 0))
    b.box("MI_ChairMesh", (-27.2, 0, 64.5), (1.5, 41, 48), 0.6, rot=(-10, 0, 0))
    b.box("MI_PlasticBlack", (-25.5, 0, 68), (3.0, 34, 10), 1.4, rot=(-10, 0, 0))
    # armrests
    for y in (-25, 25):
        b.boxr("MI_PlasticBlack", -6, -2, y - 1.6, y + 1.6, 46, 68, 1.0)
        b.boxr("MI_PlasticBlack", -14, 12, y - 3.5, y + 3.5, 67, 70.5, 1.5)
    b.colbox(-28, 28, -28, 28, 0, 55)
    b.colbox(-33, -23, -24, 24, 55, 115)
    return b.save()


def mk_monitor():
    b = Builder("SM_Monitor", "Office")
    b.boxr("MI_PlasticBlack", -14, 6, -11, 11, 0, 1.4, 0.6)
    b.box("MI_PlasticBlack", (-10.5, 0, 1.2), (3, 7, 13), 1.0)
    b.boxr("MI_PlasticBlack", -9, -6, -27.5, 27.5, 12, 45.5, 0.9)
    b.boxr("MI_PlasticBlack", -12, -8.5, -6, 6, 13, 30, 1.0)
    b.box("MI_ScreenOn", (-5.8, 0, 13.4), (0.3, 53, 30), 0.0, fill_uv=True)
    b.boxr("MI_LedGreen", -5.9, -5.7, 23, 23.8, 12.4, 13.0, 0.0)
    # sticky notes on the bezel
    b.box("MI_StickyYellow", (-5.6, 24, 38), (0.2, 6.5, 6.5), 0.0, rot=(0, 0, 6))
    b.box("MI_StickyPink", (-5.6, -25, 14), (0.2, 6, 6), 0.0, rot=(0, 0, -8))
    return b.save()


def mk_keyboard():
    b = Builder("SM_Keyboard", "Office")
    b.box("MI_PlasticBlack", (0, 0, 0), (14, 44, 1.6), 0.6, rot=(-3, 0, 0))
    for row in range(5):
        x = -5.2 + row * 2.6
        n = 15 if row < 4 else 9
        width = 2.2 if row < 4 else 2.2
        for k in range(n):
            if row == 4 and k == 4:
                b.boxr("MI_PlasticGray", x - 1.0, x + 1.0, -8, 8, 1.4 + row * 0.13, 2.3 + row * 0.13, 0.3)
                continue
            y = -19.6 + k * 2.8 if row < 4 else -19.6 + k * 4.9
            if row == 4 and k > 4:
                y += 9
            b.boxr("MI_PlasticGray", x - 1.0, x + 1.0, y - width / 2, y + width / 2, 1.4 + row * 0.13, 2.3 + row * 0.13, 0.25)
    return b.save()


def mk_mouse():
    b = Builder("SM_Mouse", "Office")
    b.sphere("MI_PlasticBlack", (0, 0, 0.2), 5.5, (1.0, 0.62, 0.38), 6)
    b.boxr("MI_PlasticGray", 2.5, 3.6, -0.3, 0.3, 1.6, 2.2, 0.1)
    return b.save()


def mk_desk_phone():
    b = Builder("SM_DeskPhone", "Office")
    b.profile_x("MI_PlasticBlack", [(-9, 0), (10, 0), (10, 2.5), (-9, 8)], -10, 10, bevel=0.6)
    b.capsule_like = None
    # handset on the left
    b.box("MI_PlasticBlack", (-1, 7.2, 5.4), (19, 5, 2.8), 1.2, rot=(-14, 0, 0))
    b.sphere("MI_PlasticBlack", (8.2, 7.2, 3.4), 2.6, (1.2, 1.0, 0.9), 4)
    b.sphere("MI_PlasticBlack", (-8.6, 7.2, 8.0), 2.6, (1.2, 1.0, 0.9), 4)
    # keypad + display
    for r in range(4):
        for c in range(3):
            b.box("MI_PlasticGray", (1.5 + r * 2.0, -6 + c * 2.4, 3.2 - r * 0.42), (1.4, 1.8, 0.8), 0.25, rot=(-17, 0, 0))
    b.box("MI_PrinterScreen", (-4.5, -2, 5.8), (3.5, 8, 0.4), 0.1, rot=(-17, 0, 0))
    b.box("MI_AccentRed", (5, 2.6, 2.4), (1.5, 1.5, 0.8), 0.2, rot=(-17, 0, 0))
    return b.save(recenter=True)


def mk_pencil_cup():
    b = Builder("SM_PencilCup", "Office")
    b.revolve("MI_MetalDark", [(0, 0), (3.6, 0), (3.8, 0.4), (3.8, 10), (3.4, 10), (3.4, 0.8), (0, 0.8)], steps=20)
    for i, (m, a, t) in enumerate([("MI_AccentYellow", 0, 8), ("MI_AccentYellow", 120, -10), ("MI_AccentBlue", 230, 12),
                                   ("MI_AccentRed", 300, -6)]):
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        b.cyl(m, (ca * 1.5, sa * 1.5, 1), 0.45, 16.5, 6, rot=(t, a, 0))
    return b.save()


def mk_sticky_pad():
    b = Builder("SM_StickyPad", "Office")
    b.boxr("MI_StickyYellow", -3.8, 3.8, -3.8, 3.8, 0, 1.2, 0.15)
    b.box("MI_StickyPink", (9, 2, 0), (7.6, 7.6, 0.15), 0.0, rot=(0, 18, 0))
    b.box("MI_Paper", (3, 12, 0), (21, 29.7, 0.3), 0.0, rot=(0, -12, 0))
    return b.save(recenter=True)


def mk_plant_small():
    b = Builder("SM_Plant_Small", "Office")
    b.revolve("MI_PotWhite", [(0, 0), (4.2, 0), (5.2, 7.5), (5.6, 8.5), (4.9, 8.5), (4.6, 7.8), (0, 7.8)], steps=20)
    b.cyl("MI_Soil", (0, 0, 6.6), 4.6, 1.0, 16)
    rnd = random.Random(5)
    for ring, (n, r, tilt, size) in enumerate([(7, 2.6, 55, 3.4), (5, 1.4, 30, 2.8), (3, 0.4, 10, 2.2)]):
        for k in range(n):
            a = k * 360.0 / n + ring * 20
            ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
            b.sphere("MI_Succulent", (ca * r, sa * r, 8.2 + ring * 0.9), size, (1.0, 0.45, 0.28), 4, rot=(tilt, a, 0))
    return b.save()


def leaf_blade(b, mat, base, angle, tilt, length, width, bend):
    ca, sa = math.cos(math.radians(angle)), math.sin(math.radians(angle))
    path = []
    for i in range(5):
        t = i / 4.0
        out = math.sin(math.radians(tilt)) * length * t + bend * t * t
        up = math.cos(math.radians(tilt)) * length * t - bend * 0.3 * t * t
        path.append((base[0] + ca * out, base[1] + sa * out, base[2] + up))
    poly = [(-width / 2, 0), (0, width * 0.12), (width / 2, 0), (0, -width * 0.12)]
    b.sweep(mat, poly, path, end_scale=0.08)


def mk_plant_large():
    b = Builder("SM_Plant_Large", "Office")
    b.revolve("MI_PotWhite", [(0, 0), (15, 0), (19, 38), (20.5, 40), (18.5, 40), (17.8, 37), (0, 37)], steps=28)
    b.cyl("MI_Soil", (0, 0, 33), 17.6, 2.0, 24)
    rnd = random.Random(9)
    for k in range(16):
        a = k * 360.0 / 16 + rnd.uniform(-10, 10)
        r = rnd.uniform(2, 9)
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        leaf_blade(b, "MI_Plant", (ca * r, sa * r, 34), a, rnd.uniform(4, 22), rnd.uniform(55, 95), rnd.uniform(5, 7.5), rnd.uniform(3, 12))
    b.colbox(-20, 20, -20, 20, 0, 120)
    return b.save()


def mk_plant_bush():
    b = Builder("SM_Plant_Bush", "Office")
    b.revolve("MI_Pot", [(0, 0), (16, 0), (18, 30), (19.5, 32), (17.5, 32), (16.8, 30), (0, 30)], steps=28)
    b.cyl("MI_Soil", (0, 0, 27), 16.5, 2.0, 24)
    b.cyl("MI_TreeBark", (0, 0, 28), 2.2, 70, 10, rot=(3, 0, 0))
    rnd = random.Random(13)
    for k in range(9):
        a = rnd.uniform(0, 360)
        r = rnd.uniform(6, 20)
        z = rnd.uniform(85, 135)
        b.sphere("MI_Plant", (math.cos(math.radians(a)) * r, math.sin(math.radians(a)) * r, z), rnd.uniform(16, 24),
                 (1.0, 1.0, 0.85), 5)
    b.colbox(-20, 20, -20, 20, 0, 150)
    return b.save()


def mk_wall_clock():
    b = Builder("SM_WallClock", "Office")
    # face towards +X, back at x=0
    # rounded-square case (a square face keeps the clock texture's UVs exact)
    b.boxr("MI_PlasticBlack", 0, 4.5, -17, 17, -17, 17, 3.0)
    b.box("MI_ClockFace", (4.4, 0, -14.5), (0.3, 29, 29), 0.0, fill_uv=True)
    b.box("MI_PlasticBlack", (4.9, 0, 0), (0.3, 1.2, 9), 0.0, rot=(0, 0, 55))
    b.box("MI_PlasticBlack", (5.1, 0, 0), (0.3, 0.8, 12.5), 0.0, rot=(0, 0, -100))
    b.box("MI_AccentRed", (5.3, 0, 0), (0.2, 0.35, 13), 0.0, rot=(0, 0, 160))
    b.cyl("MI_PlasticBlack", (5.0, 0, 0), 0.9, 0.7, 12, rot=(-90, 0, 0))
    return b.save()


def mk_poster_frame():
    b = Builder("SM_PosterFrame", "Office")
    w, h = 62.0, 92.0
    # slot 0 frame, slot 1 art (override per actor)
    b.boxr("MI_MetalDark", 0, 2.5, -w / 2, w / 2, -h / 2, -h / 2 + 2.5, 0.4)
    b.mid("MI_Poster_Synergy")
    b.boxr("MI_MetalDark", 0, 2.5, -w / 2, w / 2, h / 2 - 2.5, h / 2, 0.4)
    b.boxr("MI_MetalDark", 0, 2.5, -w / 2, -w / 2 + 2.5, -h / 2, h / 2, 0.4)
    b.boxr("MI_MetalDark", 0, 2.5, w / 2 - 2.5, w / 2, -h / 2, h / 2, 0.4)
    b.box("MI_PaperBack" if False else "MI_Paper", (0.6, 0, -h / 2 + 2.5), (1.0, w - 5, h - 5), 0.0)
    b.box("MI_Poster_Synergy", (1.3, 0, -h / 2 + 3), (0.2, w - 6, h - 6), 0.0, fill_uv=True)
    return b.save()


def mk_filing_cabinet():
    b = Builder("SM_FilingCabinet", "Office")
    b.boxr("MI_MetalGray", -33, 31, -23, 23, 2, 132, 1.2, col=True)
    b.boxr("MI_PlasticBlack", -31, 29, -21, 21, 0, 2, 0.4)
    for i in range(4):
        z0 = 4 + i * 32
        b.boxr("MI_MetalGray", 31, 33.2, -22, 22, z0 + 0.4, z0 + 31.4, 0.8)
        b.box("MI_Chrome", (34, 0, z0 + 20), (1.8, 13, 1.8), 0.5)
        b.boxr("MI_Chrome", 33.0, 33.6, -4, 4, z0 + 24, z0 + 28, 0.2)
        b.boxr("MI_Paper", 33.4, 33.7, -3.4, 3.4, z0 + 24.6, z0 + 27.4, 0.0)
        b.boxr("MI_Chrome", 33.0, 34.0, -1, 1, z0 + 6, z0 + 8, 0.2)
    return b.save()


def mk_printer_floor():
    b = Builder("SM_Printer_Floor", "Office")
    # cabinet with paper trays
    b.boxr("MI_PlasticGray", -28, 28, -30, 30, 4, 48, 1.4, col=True)
    b.boxr("MI_PlasticBlack", -26, 26, -28, 28, 0, 4, 0.5)
    for i in range(3):
        z = 8 + i * 13.3
        b.boxr("MI_PlasticWhite", 27.6, 29.2, -29, 29, z, z + 12.3, 0.7)
        b.boxr("MI_PlasticBlack", 29.0, 29.8, -9, 9, z + 8.5, z + 10.3, 0.3)
    # main body
    b.boxr("MI_PlasticWhite", -28, 28, -30, 30, 48, 86, 1.6, col=True)
    b.boxr("MI_PlasticGray", 27.6, 29.0, -29, 29, 50, 62, 0.6)
    # output bay (dark recess) with paper
    b.boxr("MI_PlasticBlack", 10, 28.6, -26, 22, 70, 82, 0.6)
    b.box("MI_Paper", (20, -3, 71.5), (16, 22, 1.2), 0.2, rot=(4, 0, 0))
    # scanner + document feeder
    b.boxr("MI_PlasticWhite", -28, 26, -30, 30, 86, 95, 1.4, col=True)
    b.boxr("MI_PlasticGray", -26, 18, -28, 18, 95, 101, 1.4)
    b.box("MI_PlasticGray", (0, -5, 101), (30, 34, 3), 1.0, rot=(-6, 0, 0))
    b.box("MI_Paper", (2, -5, 103.5), (24, 21, 0.6), 0.1, rot=(-6, 0, 0))
    # control panel
    b.box("MI_PlasticBlack", (30, 18, 86), (12, 20, 2.4), 0.8, rot=(-25, 0, 0))
    b.box("MI_PrinterScreen", (30.5, 15, 87.6), (7, 9, 0.4), 0.1, rot=(-25, 0, 0))
    b.box("MI_LedGreen", (31, 25, 87.3), (1.2, 1.2, 0.5), 0.1, rot=(-25, 0, 0))
    b.box("MI_AccentRed", (28, -24, 88.2), (2.5, 3, 0.8), 0.2)
    return b.save()


def mk_printer_desktop():
    b = Builder("SM_Printer_Desktop", "Office")
    b.boxr("MI_PlasticWhite", -20, 18, -21, 21, 0, 22, 1.6, col=True)
    b.boxr("MI_PlasticBlack", 6, 18.8, -16, 16, 6, 12, 0.6)
    b.box("MI_PlasticGray", (-4, 0, 22), (26, 34, 3), 1.0, rot=(-12, 0, 0))
    b.box("MI_Paper", (-6, 0, 24.5), (21, 29.7, 0.4), 0.1, rot=(-25, 0, 0))
    b.box("MI_PlasticGray", (-14, 0, 21), (6, 36, 12), 1.0, rot=(-20, 0, 0))
    b.box("MI_PlasticBlack", (17.8, 13, 16), (1.6, 6, 4), 0.4)
    b.boxr("MI_LedGreen", 19, 19.4, 9, 10, 18, 19, 0.0)
    b.box("MI_Paper", (22, 0, 6.5), (10, 21, 0.5), 0.1)
    b.boxr("MI_PlasticGray", 17, 28, -11, 11, 6, 6.6, 0.2)
    return b.save()


def mk_security_camera():
    """Wall-mounted dome-less security camera: bracket at x=0 (wall), body pointing +X and slightly down."""
    b = Builder("SM_SecurityCamera", "Office")
    b.boxr("MI_PlasticWhite", 0, 2.5, -6, 6, -6, 6, 0.6, col=True)
    b.box("MI_MetalGray", (6, 0, -1.5), (10, 3, 3), 0.4)
    b.box("MI_PlasticWhite", (16, 0, -9), (22, 11, 10), 1.2, rot=(-18, 0, 0))
    b.box("MI_PlasticBlack", (27, 0, -12.2), (3, 7.5, 7), 0.6, rot=(-18, 0, 0))
    b.cyl("MI_GlassDark", (28.4, 0, -9.8), 2.4, 1.2, 16, rot=(-90 + 18, 0, 0))
    b.boxr("MI_LedRed", 22, 23, -4.4, -3.4, -2.6, -1.8, 0.0)
    b.colbox(0, 30, -7, 7, -16, 6)
    return b.save()


def mk_badge_reader():
    """Door badge reader: a slim panel on the wall (back at x=0), facing +X, bottom at z=0."""
    b = Builder("SM_BadgeReader", "Office")
    b.boxr("MI_PlasticBlack", 0, 3.2, -5, 5, 0, 15, 0.8, col=True)
    b.box("MI_GlassDark", (3.25, 0, 7.5), (0.2, 7.5, 5.5), 0.0)
    b.boxr("MI_LedGreen", 3.2, 3.5, -1.2, 1.2, 4.6, 5.4, 0.0)
    b.boxr("MI_PlasticGray", 3.1, 3.4, -3.5, 3.5, 1.5, 2.3, 0.0)
    b.boxr("MI_LedRed", 3.2, 3.5, 2.8, 3.6, 12.6, 13.4, 0.0)
    return b.save()


def mk_security_monitor():
    """Desk CRT showing a 2x2 grid of camera feeds, facing +X, bottom at z=0."""
    b = Builder("SM_SecurityMonitor", "Office")
    b.boxr("MI_PlasticBeige", -22, 12, -24, 24, 0, 3, 0.8, col=True)
    b.boxr("MI_PlasticBeige", -24, 10, -23, 23, 3, 40, 2.4, col=True)
    for gy in (-1, 1):
        for gz in (0, 1):
            b.box("MI_ScreenOn", (10.2, gy * 9.6, 7 + gz * 15.4), (0.3, 18.4, 14.2), 0.0, fill_uv=True)
    b.boxr("MI_PlasticBlack", 10, 10.6, -0.6, 0.6, 6, 37, 0.0)
    b.boxr("MI_PlasticBlack", 10, 10.6, -20, 20, 22, 22.8, 0.0)
    b.boxr("MI_LedGreen", 10.2, 10.6, 18, 20, 3.6, 4.6, 0.0)
    return b.save()


def mk_inventory_clipboard():
    """Inventory sheet on a clipboard hanging on the wall (back at x=0), facing +X, bottom at z=0."""
    b = Builder("SM_InventoryClipboard", "Office")
    b.boxr("MI_WoodDark", 0, 0.8, -12, 12, 0, 34, 0.3, col=True)
    b.boxr("MI_Paper", 0.8, 1.1, -10.5, 10.5, 1.5, 30, 0.0)
    for k in range(7):
        z = 6 + k * 3.2
        b.boxr("MI_PlasticBlack", 1.1, 1.2, -9, 9 - (k % 3) * 2.5, z, z + 0.35, 0.0)
    b.boxr("MI_AccentRed", 1.1, 1.2, -9, 9, 26, 27.2, 0.0)
    b.boxr("MI_Chrome", 0.6, 2.2, -5, 5, 29.5, 34.5, 0.3)
    b.cyl("MI_PlasticBlack", (1.6, -11.5, 18), 0.6, 0.8, 8, rot=(0, 90, 0))
    return b.save()


def mk_inbox_tray():
    b = Builder("SM_InboxTray", "Office")
    for i in range(3):
        z = i * 7.5
        b.boxr("MI_PlasticBlack", -16, 16, -12.5, 12.5, z, z + 0.8, 0.3)
        b.boxr("MI_PlasticBlack", -16, -14.8, -12.5, 12.5, z, z + 5.5, 0.3)
        for y in (-12.5, 11.3):
            b.box("MI_PlasticBlack", (0, y + 0.6, z), (32, 1.2, 4.0), 0.3)
        b.box("MI_Paper", (-1 + i, (i - 1) * 0.6, z + 0.8), (29.7, 21, 1.0 + i * 0.6), 0.1, rot=(0, (i - 1) * 3, 0))
        for p in (-13, 13):
            pass
    for x in (-15, 15):
        for y in (-12, 12):
            b.boxr("MI_MetalDark", x - 0.6, x + 0.6, y - 0.6, y + 0.6, 0, 16, 0.2)
    b.colbox(-16, 16, -13, 13, 0, 17)
    return b.save()


def mk_supply_shelf():
    b = Builder("SM_SupplyShelf", "Office")
    w, d, h = 120.0, 45.0, 200.0
    for x in (-d / 2 + 1.5, d / 2 - 1.5):
        for y in (-w / 2 + 1.5, w / 2 - 1.5):
            b.boxr("MI_MetalGray", x - 1.5, x + 1.5, y - 1.5, y + 1.5, 0, h, 0.5)
    rnd = random.Random(21)
    shelves = [8, 52, 96, 140, 184]
    for z in shelves:
        b.boxr("MI_MetalGray", -d / 2, d / 2, -w / 2, w / 2, z, z + 2.5, 0.6)
    for si, z in enumerate(shelves[:-1]):
        y = -w / 2 + 4
        while y < w / 2 - 14:
            kind = rnd.random()
            if kind < 0.45:
                bw, bd, bh, mat = rnd.uniform(22, 34), rnd.uniform(28, 38), rnd.uniform(18, 32), "MI_Cardboard"
            elif kind < 0.75:
                bw, bd, bh, mat = 21.0, 29.7, rnd.choice([5.5, 11, 16.5]), "MI_Paper"
            else:
                bw, bd, bh, mat = rnd.uniform(5, 8), 30, 31, rnd.choice(["MI_Books_A", "MI_Books_B", "MI_Books_C", "MI_AccentRed"])
            bh = min(bh, 40)
            if y + bw > w / 2 - 3:
                break
            b.boxr(mat, -bd / 2 + rnd.uniform(-2, 2), bd / 2, y, y + bw, z + 2.5, z + 2.5 + bh, 0.6 if mat == "MI_Cardboard" else 0.3)
            if mat == "MI_Cardboard":
                b.boxr("MI_AccentYellow", bd / 2 - 0.2, bd / 2 + 0.1, y + 2, y + bw - 2, z + 2.5 + bh * 0.45, z + 2.5 + bh * 0.55, 0.0)
            y += bw + rnd.uniform(1, 4)
    b.colbox(-d / 2, d / 2, -w / 2, w / 2, 0, h)
    return b.save()


def mk_credenza():
    b = Builder("SM_Credenza", "Office")
    b.boxr("MI_MetalGray", -22, 21, -80, 80, 4, 72, 1.0, col=True)
    b.boxr("MI_PlasticBlack", -20, 19, -78, 78, 0, 4, 0.4)
    b.boxr("MI_WoodTop", -23, 23.5, -81, 81, 72, 75, 0.8, col=True)
    for i in range(4):
        y0 = -79 + i * 39.5
        b.boxr("MI_MetalGray", 21, 22.8, y0 + 0.4, y0 + 39.1, 6, 70, 0.6)
        b.box("MI_Chrome", (23.4, y0 + (34 if i % 2 == 0 else 5), 50), (1.5, 1.5, 14), 0.4)
    return b.save()


def mk_bookshelf():
    b = Builder("SM_Bookshelf", "Office")
    w, d, h = 100.0, 34.0, 200.0
    b.boxr("MI_WoodDark", -d / 2, d / 2, -w / 2, -w / 2 + 2.5, 0, h, 0.6)
    b.boxr("MI_WoodDark", -d / 2, d / 2, w / 2 - 2.5, w / 2, 0, h, 0.6)
    b.boxr("MI_WoodDark", -d / 2, -d / 2 + 1.5, -w / 2, w / 2, 0, h, 0.4)
    rnd = random.Random(31)
    for z in (0, 40, 80, 120, 160, 197):
        b.boxr("MI_WoodDark", -d / 2, d / 2, -w / 2 + 2.5, w / 2 - 2.5, z, z + 3, 0.5)
    for z in (3, 43, 83, 123, 163):
        y = -w / 2 + 3.5
        while y < w / 2 - 6:
            bw = rnd.uniform(2.5, 5.5)
            bh = rnd.uniform(22, 33)
            if rnd.random() < 0.12:
                y += rnd.uniform(6, 12)
                continue
            mat = rnd.choice(["MI_Books_A", "MI_Books_B", "MI_Books_C", "MI_Paper", "MI_AccentRed"])
            if y + bw > w / 2 - 3:
                break
            lean = rnd.uniform(-4, 4) if rnd.random() < 0.15 else 0
            b.box(mat, (rnd.uniform(-2, 1), y + bw / 2, z), (24, bw, bh), 0.35, rot=(0, 0, lean))
            y += bw + 0.3
    b.colbox(-d / 2, d / 2, -w / 2, w / 2, 0, h)
    return b.save()


def mk_manager_desk():
    b = Builder("SM_ManagerDesk", "Office")
    b.boxr("MI_WoodDark", -45, 45, -95, 95, 72, 76, 1.2, col=True)
    for y in (-92, 92):
        b.boxr("MI_WoodDark", -43, 43, y - 3, y + 3, 0, 72, 1.0, col=True)
    b.boxr("MI_WoodDark", -43, -38, -89, 89, 15, 72, 1.0)
    # drawer pedestal on the sitter side
    b.boxr("MI_WoodDark", -38, 40, 45, 89, 0, 72, 1.0)
    for i in range(3):
        z0 = 3 + i * 23
        b.boxr("MI_WoodDark", 40, 42, 46, 88, z0, z0 + 22, 0.8)
        b.box("MI_Chrome", (42.8, 67, z0 + 15), (1.2, 12, 1.2), 0.3)
    b.boxr("MI_AccentRed", -46, -45.2, -60, 60, 60, 62, 0.0)
    return b.save()


def mk_meeting_table():
    b = Builder("SM_MeetingTable", "Office")
    poly = []
    hx, hy, r = 55.0, 120.0, 25.0
    for cx, cy, a0 in ((hx - r, hy - r, 0), (-hx + r, hy - r, 90), (-hx + r, -hy + r, 180), (hx - r, -hy + r, 270)):
        for k in range(5):
            a = math.radians(a0 + k * 22.5)
            poly.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    b.extrude("MI_WoodTop", poly, 71.5, 3.0, bevel=1.0)
    b.colbox(-hx, hx, -hy, hy, 0, 74.5)
    for y in (-75, 75):
        b.boxr("MI_MetalDark", -6, 6, y - 4, y + 4, 2, 71.5, 1.0)
        b.boxr("MI_MetalDark", -38, 38, y - 5, y + 5, 0, 3, 1.2)
    b.boxr("MI_MetalDark", -3, 3, -75, 75, 60, 66, 0.8)
    b.box("MI_Paper", (10, -30, 74.5), (21, 29.7, 0.8), 0.1, rot=(0, 12, 0))
    b.box("MI_Paper", (-15, 40, 74.5), (21, 29.7, 0.5), 0.1, rot=(0, -20, 0))
    return b.save()


def mk_whiteboard():
    b = Builder("SM_Whiteboard", "Office")
    b.boxr("MI_Aluminum", 0, 2.5, -92, 92, -47, 47, 0.8)
    b.boxr("MI_PlasticWhite", 2.4, 2.8, -89, 89, -44, 44, 0.0)
    rnd = random.Random(91)
    for i in range(7):
        y = rnd.uniform(-80, 20)
        b.boxr(rnd.choice(["MI_AccentBlue", "MI_AccentRed", "MI_PlasticBlack"]), 2.8, 2.9, y, y + rnd.uniform(15, 60),
               30 - i * 10, 31 - i * 10, 0.0)
    b.boxr("MI_AccentRed", 2.8, 2.9, 35, 75, -20, 20, 0.0)
    b.boxr("MI_PlasticWhite", 2.9, 3.0, 37, 73, -18, 18, 0.0)
    b.boxr("MI_Aluminum", 0, 7, -60, 60, -50, -47, 0.5)
    for k, m in enumerate(["MI_AccentRed", "MI_AccentBlue", "MI_PlasticBlack"]):
        b.cyl(m, (4.5, -20 + k * 8, -46.2), 0.8, 12, 8, rot=(0, 0, 90))
    return b.save()


def mk_rug():
    b = Builder("SM_Rug", "Office")
    b.boxr("MI_FabricRed", -100, 100, -140, 140, 0, 0.8, 0.3)
    b.boxr("MI_Fabric", -88, 88, -128, 128, 0.8, 0.9, 0.0)
    return b.save()


def mk_sofa():
    b = Builder("SM_Sofa", "Office")
    W, D = 200.0, 88.0
    b.boxr("MI_FabricBlue", -D / 2, D / 2, -W / 2 + 18, W / 2 - 18, 10, 40, 4.0, col=True)
    for y0, y1 in ((-W / 2 + 18, -1), (1, W / 2 - 18)):
        b.boxr("MI_FabricBlue", -D / 2 + 22, D / 2, y0 + 0.5, y1 - 0.5, 40, 54, 5.0)
    b.box("MI_FabricBlue", (-D / 2 + 11, 0, 38), (20, W - 36, 44), 6.0, rot=(-6, 0, 0))
    for y in (-W / 2 + 9, W / 2 - 9):
        b.boxr("MI_FabricBlue", -D / 2, D / 2, y - 9, y + 9, 10, 64, 5.0, col=True)
    for x in (-D / 2 + 6, D / 2 - 6):
        for y in (-W / 2 + 6, W / 2 - 6):
            b.boxr("MI_PlasticBlack", x - 2, x + 2, y - 2, y + 2, 0, 10, 0.5)
    b.colbox(-D / 2, -D / 2 + 22, -W / 2, W / 2, 10, 82)
    return b.save()


def mk_coffee_table_low():
    b = Builder("SM_CoffeeTableLow", "Office")
    b.boxr("MI_WoodTop", -30, 30, -60, 60, 38, 42, 1.4, col=True)
    for x in (-25, 25):
        for y in (-55, 55):
            b.boxr("MI_MetalDark", x - 1.5, x + 1.5, y - 1.5, y + 1.5, 0, 38, 0.5)
    b.boxr("MI_MetalDark", -26, 26, -56, 56, 12, 14, 0.4)
    b.box("MI_Paper", (5, -20, 42), (21, 29.7, 1.2), 0.2, rot=(0, 17, 0))
    b.box("MI_AccentRed", (-8, 25, 42), (21, 28, 1.6), 0.3, rot=(0, -9, 0))
    return b.save()


def mk_reception_counter():
    b = Builder("SM_ReceptionCounter", "Office")
    W = 280.0
    # visitor-facing front panel (+X) with an accent stripe, high transaction ledge on top
    b.boxr("MI_WoodTop", 30, 37, -W / 2, W / 2, 0, 106, 1.2, col=True)
    b.boxr("MI_AccentRed", 37, 37.8, -W / 2 + 6, W / 2 - 6, 18, 26, 0.2)
    b.boxr("MI_Cabinet_Cream", 20, 52, -W / 2, W / 2, 106, 110, 1.2, col=True)
    for y in (-W / 2 + 3, W / 2 - 3):
        b.boxr("MI_WoodTop", -40, 37, y - 3, y + 3, 0, 106, 1.0, col=True)
    # staff work surface behind
    b.boxr("MI_Cabinet_Cream", -42, 30, -W / 2 + 6, W / 2 - 6, 72, 75, 0.9)
    b.boxr("MI_PlasticBlack", 28, 30, -W / 2 + 6, W / 2 - 6, 0, 72, 0.2)
    b.boxr("MI_MetalGray", -40, 28, W / 2 - 50, W / 2 - 6, 0, 70, 0.8)
    return b.save()


def mk_company_sign():
    b = Builder("SM_CompanySignBoard", "Office")
    b.boxr("MI_Aluminum", 0, 3, -120, 120, -40, 40, 1.4)
    b.boxr("MI_AccentRed", 3, 3.6, -120, 120, -40, -32, 0.0)
    for y in (-110, 110):
        for z in (-30, 30):
            b.cyl("MI_Chrome", (3, y, z), 1.4, 1.2, 12, rot=(-90, 0, 0))
    return b.save()


def mk_counter(name, width, with_doors=True, top_mat="MI_Countertop", body="MI_Cabinet_Cream"):
    b = Builder(name, "Kitchen")
    hw = width / 2.0
    b.boxr("MI_PlasticBlack", -28, 24, -hw + 1, hw - 1, 0, 10, 0.4)
    b.boxr(body, -30, 28, -hw, hw, 10, 87, 1.0, col=True)
    n = int(round(width / 60.0))
    for i in range(n):
        y0 = -hw + i * (width / n)
        y1 = y0 + width / n
        b.boxr(body, 28, 30, y0 + 0.4, y1 - 0.4, 12, 70, 0.8)
        b.boxr(body, 28, 30, y0 + 0.4, y1 - 0.4, 71, 86, 0.8)
        b.box("MI_Stainless", (31, (y0 + y1) / 2, 80.5), (1.6, 14, 1.2), 0.4)
        b.box("MI_Stainless", (31, y1 - 6 if i % 2 == 0 else y0 + 6, 52), (1.6, 1.2, 14), 0.4)
    b.boxr(top_mat, -31.5, 31.5, -hw - 0.5, hw + 0.5, 87, 91.5, 0.8, col=True)
    b.boxr(top_mat, -31.5, -30, -hw - 0.5, hw + 0.5, 91.5, 101.5, 0.5)
    return b


def mk_coffee_counter():
    return mk_counter("SM_CoffeeCounter_240", 240.0).save()


def mk_copy_counter():
    return mk_counter("SM_CopyCounter_300", 300.0, top_mat="MI_Countertop", body="MI_MetalGray").save()


def mk_kitchen_base():
    b = Builder("SM_Kitchen_Base60", "Kitchen")
    b.boxr("MI_PlasticBlack", -28, 24, -29.8, 29.8, 0, 10, 0.3)
    b.boxr("MI_Cabinet_Cream", -30, 28, -30, 30, 10, 88, 0.6, col=True)
    b.boxr("MI_Cabinet_Cream", 28, 30, -29.6, 29.6, 71.5, 87.5, 0.8)
    b.boxr("MI_Cabinet_Cream", 28, 30, -29.6, 29.6, 11, 71, 0.8)
    b.box("MI_Stainless", (31, 0, 81.5), (1.6, 16, 1.2), 0.4)
    b.box("MI_Stainless", (31, 22, 52), (1.6, 1.2, 14), 0.4)
    return b.save()


def mk_kitchen_upper():
    b = Builder("SM_Kitchen_Upper60", "Kitchen")
    b.boxr("MI_Cabinet_Cream", -17.5, 15.5, -30, 30, 0, 72, 0.6, col=True)
    b.boxr("MI_Cabinet_Cream", 15.5, 17.5, -29.6, 29.6, 0.4, 71.6, 0.8)
    b.box("MI_Stainless", (18.4, 22, 4), (1.6, 1.2, 14), 0.4)
    return b.save()


def mk_microwave_niche():
    b = Builder("SM_Kitchen_MicrowaveNiche120", "Kitchen")
    b.boxr("MI_Cabinet_Cream", -20, 20, -60, -58, 0, 72, 0.5, col=True)
    b.boxr("MI_Cabinet_Cream", -20, 20, 58, 60, 0, 72, 0.5, col=True)
    b.boxr("MI_Cabinet_Cream", -20, 20, -1, 1, 0, 72, 0.5)
    b.boxr("MI_Cabinet_Cream", -20, 20, -60, 60, 0, 4, 0.5)
    b.boxr("MI_Cabinet_Cream", -20, 20, -60, 60, 40, 43, 0.5)
    b.boxr("MI_Cabinet_Cream", -20, 20, -60, 60, 69, 72, 0.5)
    b.boxr("MI_Cabinet_Cream", -20, -18, -60, 60, 0, 72, 0.3)
    b.boxr("MI_Cabinet_Cream", 18, 20, -59, 59, 43.5, 68.5, 0.8)
    b.box("MI_Stainless", (21, 0, 47), (1.6, 14, 1.2), 0.4)
    return b.save()


def mk_kitchen_shelf():
    b = Builder("SM_Kitchen_Shelf180", "Kitchen")
    b.boxr("MI_WoodTop", -12, 12, -90, 90, 0, 3, 0.8, col=True)
    for y in (-70, 70):
        b.box("MI_MetalDark", (-10, y, -12), (4, 2, 12), 0.3)
    rnd = random.Random(41)
    mats = ["MI_Mug_Red", "MI_Mug_Blue", "MI_Mug_Yellow", "MI_Mug_White", "MI_Mug_Green"]
    for i in range(9):
        y = -78 + i * 13 + rnd.uniform(-2, 2)
        m = mats[i % len(mats)]
        b.revolve(m, [(0, 0), (3.6, 0), (4.2, 9.5), (3.7, 9.5), (3.3, 0.8), (0, 0.8)], (0, y, 3), 16)
        b.torus(m, (0, y + 4.6, 8.0), 2.4, 0.6, rot=(90, 0, 0), steps=(10, 6))
    return b.save()


def mk_countertop_sink():
    b = Builder("SM_Countertop_Sink_900", "Kitchen")
    hw = 450.0
    hole = (-22.0, 18.0, -40.0, 40.0)  # x0, x1, y0, y1 (sink cut-out)
    x0, x1, y0, y1 = hole
    b.boxr("MI_Countertop", -31.5, 31.5, -hw, y0, 0, 3.5, 0.8, col=True)
    b.boxr("MI_Countertop", -31.5, 31.5, y1, hw, 0, 3.5, 0.8, col=True)
    b.boxr("MI_Countertop", x1, 31.5, y0, y1, 0, 3.5, 0.4)
    b.boxr("MI_Countertop", -31.5, x0, y0, y1, 0, 3.5, 0.4)
    b.boxr("MI_Countertop", -31.5, -30, -hw, hw, 3.5, 13.5, 0.5)
    # tiled backsplash strip
    b.boxr("MI_KitchenTile", -31.5, -30.5, -hw, hw, 13.5, 55, 0.0)
    return b.save()


def mk_sink_double():
    b = Builder("SM_Sink_Double", "Kitchen")
    # rim at z 0..0.8 sitting on the counter, basins hang below
    b.boxr("MI_Stainless", -21, 19, -41, 41, 0, 0.8, 0.3)
    for yc in (-19.5, 19.5):
        x0, x1, y0, y1 = -18, 16, yc - 17.5, yc + 17.5
        b.boxr("MI_Stainless", x0, x1, y0, y1, -19, -18, 0.3)
        b.boxr("MI_Stainless", x0, x0 + 0.8, y0, y1, -19, 0.8, 0.2)
        b.boxr("MI_Stainless", x1 - 0.8, x1, y0, y1, -19, 0.8, 0.2)
        b.boxr("MI_Stainless", x0, x1, y0, y0 + 0.8, -19, 0.8, 0.2)
        b.boxr("MI_Stainless", x0, x1, y1 - 0.8, y1, -19, 0.8, 0.2)
        b.cyl("MI_PlasticBlack", (0, yc, -18.1), 2.2, 0.3, 12)
    # faucet (gooseneck)
    b.cyl("MI_Chrome", (-19.5, 0, 0.8), 2.2, 6, 16)
    path = [(-19.5, 0, 6)]
    for i in range(1, 9):
        a = math.radians(i * 22.5)
        path.append((-19.5 + 9 * (1 - math.cos(a)), 0, 6 + 22 + 9 * math.sin(a) - 22 * (1 - i / 8.0) * 0))
    path = [(-19.5, 0, 6), (-19.5, 0, 26)] + [(-19.5 + 9 - 9 * math.cos(math.radians(a)), 0, 26 + 9 * math.sin(math.radians(a)))
                                             for a in (30, 60, 90, 120, 150)] + [(-1.5, 0, 22)]
    circle = [(math.cos(math.radians(a)) * 1.2, math.sin(math.radians(a)) * 1.2) for a in range(0, 360, 45)]
    b.sweep("MI_Chrome", circle, path)
    b.box("MI_Chrome", (-19.5, 4, 8), (2, 7, 1.4), 0.4, rot=(-15, 0, 0))
    b.colbox(-21, 19, -41, 41, 0, 2)
    return b.save()


def mk_dish_rack():
    b = Builder("SM_DishRack", "Kitchen")
    for x in (-17, 17):
        b.boxr("MI_Chrome", x - 0.4, x + 0.4, -22, 22, 0, 9, 0.1)
    for y in (-22, 22):
        b.boxr("MI_Chrome", -17, 17, y - 0.4, y + 0.4, 0, 9, 0.1)
    for i in range(9):
        y = -20 + i * 5
        b.boxr("MI_Chrome", -17, 17, y - 0.3, y + 0.3, 1, 1.6, 0.1)
    for i, m in enumerate(["MI_PlasticWhite", "MI_PlasticWhite", "MI_AccentBlue", "MI_PlasticWhite"]):
        b.cyl(m, (-4, -14 + i * 5.5, 12), 12.5, 0.8, 24, rot=(0, 0, 90 - 12))
    b.revolve("MI_Mug_Red", [(0, 0), (3.6, 0), (4.2, 9.5), (3.7, 9.5), (3.3, 0.8), (0, 0.8)], (10, 12, 1.6), 16, rot=(180, 0, 0))
    return b.save()


def mk_microwave():
    b = Builder("SM_Microwave", "Kitchen")
    b.boxr("MI_Stainless", -18, 18, -25, 25, 0, 29, 1.4, col=True)
    b.boxr("MI_PlasticBlack", 17.5, 19, -24, 13, 1.5, 27.5, 0.8)
    b.boxr("MI_GlassDark", 18.9, 19.3, -20, 8, 5, 24, 0.0)
    b.boxr("MI_PlasticBlack", 17.5, 19, 13.4, 24, 1.5, 27.5, 0.6)
    b.boxr("MI_LedGreen", 19, 19.3, 15.5, 22, 22, 25, 0.0)
    for r in range(4):
        for c in range(3):
            b.boxr("MI_PlasticGray", 19, 19.6, 15 + c * 2.6, 17 + c * 2.6, 9 + r * 3, 11 + r * 3, 0.2)
    b.box("MI_Stainless", (20, 11, 6), (1.8, 1.6, 17), 0.5)
    return b.save()


def mk_fridge():
    b = Builder("SM_Fridge", "Kitchen")
    b.boxr("MI_Stainless", -36, 32, -40, 40, 6, 185, 2.0, col=True)
    b.boxr("MI_PlasticBlack", -34, 30, -38, 38, 0, 6, 0.4)
    b.boxr("MI_Stainless", 32, 35.5, -39.6, 39.6, 70, 184, 1.6)
    b.boxr("MI_Stainless", 32, 35.5, -39.6, 39.6, 7, 69, 1.6)
    b.boxr("MI_PlasticBlack", 32, 34, -39, 39, 69, 70, 0.0)
    b.boxr("MI_PlasticBlack", 34.5, 36, -39, 39, 1, 6, 0.3)
    b.box("MI_Chrome", (38, 33, 100), (2, 2.4, 50), 0.8)
    b.box("MI_Chrome", (38, 33, 38), (2, 2.4, 26), 0.8)
    for z in (100, 148):
        b.box("MI_Chrome", (36.5, 33, z), (3, 2.4, 2.4), 0.6)
    b.boxr("MI_Paper", 35.5, 35.8, -20, -8, 140, 155, 0.0)
    b.boxr("MI_StickyYellow", 35.5, 35.9, 5, 12, 130, 137, 0.0)
    return b.save()


def carafe(b, x, y, z, coffee_level=7.0):
    prof = [(0, 0), (7.5, 0), (8.5, 1.5), (9, 6), (8.4, 11), (6.0, 14.5), (5.2, 16.5)]
    b.revolve("MI_GlassCarafe", prof, (x, y, z), 24)
    if coffee_level > 0:
        b.revolve("MI_Coffee", [(0, 0.4), (7.3, 0.4), (8.2, 1.8), (8.6, coffee_level), (0, coffee_level)], (x, y, z), 20)
    b.cyl("MI_PlasticBlack", (x, y, z + 15.5), 5.8, 2.8, 20, 0.6)
    b.cyl("MI_AccentRed" if coffee_level < 6 else "MI_PlasticBlack", (x, y, z + 18.0), 4.8, 1.2, 20, 0.4)
    b.box("MI_PlasticBlack", (x - 11.5, y, z + 6), (2.2, 2.6, 11), 0.8)
    b.box("MI_PlasticBlack", (x - 9.5, y, z + 15.6), (5, 2.6, 2.2), 0.8)
    b.box("MI_PlasticBlack", (x - 9.5, y, z + 5.5), (4, 2.6, 2.0), 0.8)


def mk_coffee_brewer():
    b = Builder("SM_CoffeeBrewer", "Kitchen")
    W = 52.0
    # base with two warmer plates at the front
    b.boxr("MI_Stainless", -22, 22, -W / 2, W / 2, 0, 5, 1.0, col=True)
    for y in (-12.5, 12.5):
        b.cyl("MI_PlasticBlack", (6, y, 5), 9.5, 0.6, 24, 0.2)
    # rear tower and brew head
    b.boxr("MI_Stainless", -22, -8, -W / 2, W / 2, 5, 48, 1.2, col=True)
    b.boxr("MI_Stainless", -22, 22, -W / 2, W / 2, 48, 62, 1.6, col=True)
    b.boxr("MI_PlasticBlack", 21.6, 22.6, -W / 2 + 3, W / 2 - 3, 52, 58, 0.4)
    b.boxr("MI_AccentRed", 22.4, 23.2, -20, -16, 53.5, 56.5, 0.2)
    b.boxr("MI_PlasticGray", 22.4, 23.2, -14, -10, 53.5, 56.5, 0.2)
    b.boxr("MI_LedGreen", 22.5, 23.0, 16, 18, 54.5, 55.5, 0.0)
    # brew funnel under the head (above the right warmer)
    b.revolve("MI_PlasticBlack", [(0, 0), (3, 0), (8.5, 6), (9, 8.5), (0, 8.5)], (6, 12.5, 38.5), 20)
    b.box("MI_PlasticBlack", (17, 12.5, 44), (12, 3, 2.5), 0.8)
    # second warmer on top + carafes
    b.cyl("MI_PlasticBlack", (2, -8, 62), 9.0, 0.6, 24, 0.2)
    carafe(b, 6, 12.5, 5.6, 8.0)
    carafe(b, 6, -12.5, 5.6, 3.5)
    # label
    b.boxr("MI_PlasticWhite", -8.2, -7.6, -12, 12, 30, 40, 0.0)
    return b.save()


def mk_water_cooler():
    b = Builder("SM_WaterCooler", "Kitchen")
    b.boxr("MI_PlasticWhite", -16, 16, -16, 16, 0, 98, 2.0, col=True)
    b.boxr("MI_PlasticGray", 10, 17, -12, 12, 32, 36, 0.8)
    b.boxr("MI_PlasticGray", 15.8, 16.6, -11, 11, 36, 60, 0.4)
    b.box("MI_AccentBlue", (17.5, -5, 52), (3, 3, 4), 0.6)
    b.box("MI_AccentRed", (17.5, 5, 52), (3, 3, 4), 0.6)
    b.revolve("MI_WaterBottle", [(0, 0), (4.5, 0), (5, 4), (13.5, 10), (14, 14), (13.5, 26), (14, 30), (13.5, 42), (11, 48), (0, 49)],
              (0, 0, 96), 28)
    b.colbox(-16, 16, -16, 16, 0, 145)
    return b.save()


def mk_vending():
    b = Builder("SM_VendingMachine", "Kitchen")
    W, D, H = 95.0, 85.0, 185.0
    gx0, gx1 = -W / 2 + 5, W / 2 - 30  # glass span in Y
    # cabinet shell (open behind the glass so the lit snack shelves are visible)
    b.boxr("MI_PlasticBlack", -D / 2, D / 2 - 34, -W / 2, W / 2, 0, H, 2.0)
    b.boxr("MI_PlasticBlack", -D / 2, D / 2 - 4, -W / 2, gx0, 0, H, 1.5)
    b.boxr("MI_PlasticBlack", -D / 2, D / 2 - 4, gx1, W / 2, 0, H, 1.5)
    b.boxr("MI_PlasticBlack", -D / 2, D / 2 - 4, gx0 - 1, gx1 + 1, H - 12, H, 1.5)
    b.boxr("MI_PlasticBlack", -D / 2, D / 2 - 4, gx0 - 1, gx1 + 1, 0, 40, 1.5)
    b.colbox(-D / 2, D / 2 - 4, -W / 2, W / 2, 0, H)
    b.boxr("MI_VendingGlow", D / 2 - 34, D / 2 - 33, gx0, gx1, 40, H - 12, 0.0)
    b.boxr("MI_MetalDark", D / 2 - 34, D / 2 - 4, gx0 - 1, gx0, 40, H - 12, 0.0)
    b.boxr("MI_MetalDark", D / 2 - 34, D / 2 - 4, gx1, gx1 + 1, 40, H - 12, 0.0)
    snack_mats = ["MI_AccentRed", "MI_AccentYellow", "MI_AccentGreen", "MI_AccentBlue", "MI_Mug_White", "MI_CarPaint_Red"]
    rnd = random.Random(61)
    for r in range(6):
        z = 44 + r * 21.5
        b.boxr("MI_MetalGray", D / 2 - 34, D / 2 - 6, gx0, gx1, z, z + 1, 0.2)
        n = 5
        for k in range(n):
            y = gx0 + 4 + k * (gx1 - gx0 - 8) / (n - 1)
            m = snack_mats[(r * 2 + k) % len(snack_mats)]
            if r % 2 == 0:
                b.box(m, (D / 2 - 14, y, z + 1), (3, 8.5, 13), 0.8, rot=(0, 0, rnd.uniform(-4, 4)))
            else:
                b.sphere(m, (D / 2 - 14, y, z + 8), 6, (0.45, 0.85, 1.1), 4)
            b.cyl("MI_Chrome", (D / 2 - 24, y, z + 3), 2.6, 0.4, 10, rot=(-90, 0, 0))
    b.boxr("MI_Glass", D / 2 - 5, D / 2 - 4, gx0, gx1, 40, H - 12, 0.0)
    # keypad column
    b.boxr("MI_PlasticGray", D / 2 - 4, D / 2 - 3, gx1 + 2, W / 2 - 3, 40, H - 12, 0.6)
    b.boxr("MI_LedGreen", D / 2 - 3, D / 2 - 2.6, gx1 + 6, W / 2 - 7, 140, 148, 0.0)
    for r in range(4):
        for c in range(3):
            b.boxr("MI_PlasticBlack", D / 2 - 3, D / 2 - 2, gx1 + 6 + c * 6, gx1 + 10 + c * 6, 110 + r * 6, 114 + r * 6, 0.3)
    b.boxr("MI_Chrome", D / 2 - 3, D / 2 - 2, gx1 + 9, gx1 + 17, 95, 99, 0.3)
    # header and pickup flap
    b.boxr("MI_AccentRed", D / 2 - 4, D / 2 - 3, -W / 2 + 3, W / 2 - 3, H - 11, H - 3, 0.6)
    b.boxr("MI_MetalDark", D / 2 - 4, D / 2 - 2.5, gx0, gx1, 14, 34, 0.8)
    b.boxr("MI_PlasticBlack", D / 2 - 3, D / 2 - 2, -W / 2 + 3, W / 2 - 3, 0, 8, 0.3)
    return b.save()


def mk_trash_bin():
    b = Builder("SM_TrashBin", "Kitchen")
    b.revolve("MI_PlasticGray", [(0, 0), (14, 0), (16.5, 44), (17.5, 46), (16.5, 46), (15.5, 45), (13, 2), (0, 2)], steps=24)
    b.cyl("MI_PlasticBlack", (0, 0, 40), 15.6, 0.4, 20)
    return b.save()


def mk_recycle_bin():
    b = Builder("SM_RecycleBin", "Kitchen")
    b.boxr("MI_Recycle", -17, 17, -16, 16, 0, 66, 2.2, col=True)
    b.boxr("MI_Recycle", -18, 18, -17, 17, 66, 71, 1.6)
    b.boxr("MI_PlasticBlack", -6, 6, -10, 10, 70.8, 71.4, 0.0)
    b.boxr("MI_PlasticWhite", 17, 17.4, -9, 9, 36, 54, 0.0)
    b.boxr("MI_Recycle", 17.2, 17.6, -6, 6, 39, 51, 0.0)
    return b.save()


def mk_cafe_table():
    b = Builder("SM_CafeTable", "Kitchen")
    b.cyl("MI_PlasticWhite", (0, 0, 71), 40, 3.4, 40, 1.4, col=True)
    b.cyl("MI_MetalDark", (0, 0, 65), 30, 6.0, 4, 1.0, rot=(0, 45, 0))
    b.cyl("MI_Chrome", (0, 0, 3), 3.4, 64, 16)
    b.cyl("MI_MetalDark", (0, 0, 0), 24, 3.0, 32, 1.2)
    return b.save()


def mk_plastic_chair():
    b = Builder("SM_PlasticChair", "Kitchen")
    b.boxr("MI_AccentRed", -21, 21, -21, 21, 44, 47, 1.4, col=True)
    b.box("MI_AccentRed", (-21, 0, 46), (3, 40, 38), 1.4, rot=(-9, 0, 0))
    b.box("MI_AccentRed", (-23.5, 0, 72), (4, 40, 8), 1.8, rot=(-9, 0, 0))
    for x in (-17, 17):
        for y in (-17, 17):
            b.cyl("MI_Chrome", (x, y, 0), 1.1, 44.5, 10, rot=(x * 0.18, 0, -y * 0.18))
    b.colbox(-25, -19, -21, 21, 44, 80)
    return b.save()


def mk_mug():
    b = Builder("SM_Mug", "Kitchen")
    b.revolve("MI_Mug_Red", [(0, 0), (3.8, 0), (4.3, 0.6), (4.3, 9.5), (3.8, 9.5), (3.8, 1.2), (0, 1.2)], steps=24)
    b.cyl("MI_Coffee", (0, 0, 7.4), 3.75, 0.3, 20)
    b.torus("MI_Mug_Red", (0, 4.9, 5.0), 2.6, 0.65, rot=(90, 0, 0), steps=(12, 6))
    return b.save()


# =============================================================================================
# Ceiling / doors / windows
# =============================================================================================

def mk_light_panel():
    b = Builder("SM_CeilingLightPanel", "Architecture")
    # hangs below Z=0 (ceiling surface)
    b.boxr("MI_PlasticWhite", -60, 60, -30, 30, -2.0, 0.5, 0.6)
    b.boxr("MI_LightPanel", -57, 57, -27, 27, -2.3, -1.8, 0.0)
    return b.save()


def mk_ceiling_vent():
    b = Builder("SM_CeilingVent", "Architecture")
    b.boxr("MI_PlasticWhite", -30, 30, -30, 30, -1.5, 0.5, 0.5)
    for i, s in enumerate((25, 19, 13)):
        b.boxr("MI_PlasticWhite", -s, s, -s, s, -3.0 - i * 0.8, -1.5, 0.4)
    b.boxr("MI_PlasticBlack", -8, 8, -8, 8, -5.6, -4.2, 0.2)
    return b.save()


def mk_window_module():
    """Full-height window for exterior walls. Width along local Y, interior side +X, base at floor."""
    b = Builder("SM_WindowModule", "Architecture")
    W, H = L.WINDOW_W, L.WINDOW_H
    T = L.EXT_T
    hw = W / 2.0
    fm = "MI_MetalDark"
    b.boxr(fm, -T / 2, T / 2, -hw, -hw + 6, 0, H, 0.6)
    b.boxr(fm, -T / 2, T / 2, hw - 6, hw, 0, H, 0.6)
    b.boxr(fm, -T / 2, T / 2, -hw, hw, 0, 10, 0.6)
    b.boxr(fm, -T / 2, T / 2, -hw, hw, H - 8, H, 0.6)
    b.boxr(fm, -6, 4, -hw, hw, 88, 93, 0.5)
    b.boxr(fm, -6, 4, -2.5, 2.5, 10, H - 8, 0.5)
    b.boxr("MI_Glass", -1.0, 0.0, -hw + 6, hw - 6, 10, H - 8, 0.0)
    # interior sill
    b.boxr("MI_Cabinet_Cream", T / 2 - 2, T / 2 + 8, -hw, hw, 6, 10, 0.6)
    # horizontal blinds, lowered from the head to ~1.7 m, slats tilted half open
    head_z = H - 9.0
    b.boxr("MI_PlasticWhite", 9, 15, -hw + 7, hw - 7, head_z - 5, head_z, 0.8)
    z = head_z - 7
    while z > 175:
        b.box("MI_Blinds", (12, 0, z), (2.6, W - 16, 0.25), 0.0, rot=(0, 0, 0), uv="box")
        z -= 2.4
    b.boxr("MI_PlasticWhite", 10.5, 13.5, -hw + 8, hw - 8, z - 1.5, z + 0.5, 0.5)
    for y in (-hw / 2, hw / 2):
        b.boxr("MI_PlasticWhite", 11.9, 12.1, y - 0.1, y + 0.1, z, head_z - 5, 0.0)
    b.cyl("MI_PlasticWhite", (13.5, hw - 14, 120), 0.4, head_z - 125, 6)
    b.colbox(-T / 2, T / 2, -hw, hw, 0, H)
    return b.save()


def mk_entrance_doors():
    b = Builder("SM_EntranceDoors", "Architecture")
    W, H = 250.0, L.WINDOW_H
    hw = W / 2
    T = L.EXT_T
    fm = "MI_Aluminum"
    b.boxr(fm, -T / 2, T / 2, -hw, -hw + 7, 0, H, 0.6)
    b.boxr(fm, -T / 2, T / 2, hw - 7, hw, 0, H, 0.6)
    b.boxr(fm, -T / 2, T / 2, -hw, hw, H - 40, H, 0.6)
    b.boxr("MI_Glass", -1, 0, -hw + 7, hw - 7, H - 40, H, 0.0)
    for y0, y1 in ((-hw + 7, -1), (1, hw - 7)):
        b.boxr(fm, -4, 4, y0, y1, 0, 10, 0.4)
        b.boxr(fm, -4, 4, y0, y1, H - 46, H - 40, 0.4)
        b.boxr(fm, -4, 4, y0, y0 + 5, 0, H - 40, 0.4)
        b.boxr(fm, -4, 4, y1 - 5, y1, 0, H - 40, 0.4)
        b.boxr("MI_Glass", -0.6, 0.6, y0 + 5, y1 - 5, 10, H - 46, 0.0)
    for y in (-6, 6):
        b.box("MI_Chrome", (6, y, 70), (2.5, 2.5, 90), 1.0)
        b.box("MI_Chrome", (-6, y, 70), (2.5, 2.5, 90), 1.0)
    b.colbox(-T / 2, T / 2, -hw, hw, 0, H)
    return b.save()


def mk_door_decor():
    """Closed decor door with frame; back face at X=0 against the wall. Slot 0 leaf, slot 1 sign."""
    b = Builder("SM_DoorDecor", "Architecture")
    b.mid("MI_DoorWood")
    b.mid("MI_Sign_Men")
    b.boxr("MI_MetalDark", 0, 3, -56, -51, 0, 216, 0.6)
    b.boxr("MI_MetalDark", 0, 3, 51, 56, 0, 216, 0.6)
    b.boxr("MI_MetalDark", 0, 3, -56, 56, 211, 216, 0.6)
    b.boxr("MI_DoorWood", 0, 2.2, -51, 51, 0.5, 211, 0.6)
    b.box("MI_Sign_Men", (2.2, 0, 150), (0.4, 18, 18), 0.0, fill_uv=True)
    b.boxr("MI_Stainless", 2.2, 2.6, -48, 48, 1, 20, 0.2)
    b.box("MI_Chrome", (4.5, 42, 103), (5, 2.2, 2.2), 0.6)
    b.box("MI_Chrome", (6.5, 36, 103), (2, 14, 2.2), 0.6)
    b.cyl("MI_Chrome", (2.2, 42, 103), 3.5, 1.0, 16, rot=(-90, 0, 0))
    return b.save()


def mk_door_frame(width):
    """Casing around a walkable opening in an interior wall. Wall runs along Y, opening centred at 0."""
    b = Builder("SM_DoorFrame_%d" % int(width), "Architecture")
    t = L.INT_T / 2 + 1.5
    hw = width / 2.0
    b.boxr("MI_MetalDark", -t, t, -hw - 5, -hw, 0, L.DOOR_H + 5, 0.6)
    b.boxr("MI_MetalDark", -t, t, hw, hw + 5, 0, L.DOOR_H + 5, 0.6)
    b.boxr("MI_MetalDark", -t, t, -hw - 5, hw + 5, L.DOOR_H, L.DOOR_H + 5, 0.6)
    b.boxr("MI_MetalDark", -t + 1, t - 1, -hw - 5, hw + 5, -0.5, 0.4, 0.0)
    b.colbox(-t, t, -hw - 5, -hw, 0, L.DOOR_H + 5)
    b.colbox(-t, t, hw, hw + 5, 0, L.DOOR_H + 5)
    return b.save()


# =============================================================================================
# Exterior props
# =============================================================================================

def mk_car():
    b = Builder("SM_Car_Sedan", "Exterior")
    body_mat = "MI_CarPaint_Red"
    b.mid(body_mat)
    Wd = 178.0
    # lower body side profile (x forward, z up); front at +X
    lower = [(-228, 32), (-232, 55), (-222, 82), (-150, 90), (150, 92), (218, 78), (232, 60), (228, 32), (205, 22), (-205, 22)]
    b.profile_x(body_mat, lower, -Wd / 2, Wd / 2, bevel=4.0)
    # greenhouse (cabin) narrower, glass
    cabin = [(-150, 89), (-115, 132), (40, 136), (95, 92)]
    b.profile_x("MI_GlassDark", cabin, -Wd / 2 + 12, Wd / 2 - 12, bevel=3.0)
    # roof cap in body colour
    roof = [(-108, 130), (-100, 139), (32, 141), (40, 133)]
    b.profile_x(body_mat, roof, -Wd / 2 + 16, Wd / 2 - 16, bevel=2.5)
    # pillars
    b.box(body_mat, (-8, -Wd / 2 + 13.5, 90), (8, 2.5, 46), 0.8)
    b.box(body_mat, (-8, Wd / 2 - 13.5, 90), (8, 2.5, 46), 0.8)
    for x in (-150, 145):
        for y in (-Wd / 2 + 8, Wd / 2 - 8):
            b.cyl("MI_Rubber", (x, y - 13 if y < 0 else y + 13, 33), 33, 24, 24, 3.0, rot=(0, 0, 90))
            b.cyl("MI_Chrome", (x, y - 26 if y < 0 else y + 26, 33), 19, 1.5, 16, 1.0, rot=(0, 0, 90))
    for y in (-Wd / 2 + 22, Wd / 2 - 22):
        b.box("MI_CarLightFront", (229, y, 64), (6, 32, 10), 2.0)
        b.box("MI_CarLightRear", (-229, y, 66), (6, 36, 10), 2.0)
    b.boxr("MI_PlasticBlack", 226, 236, -60, 60, 26, 40, 2.5)
    b.boxr("MI_PlasticBlack", -236, -226, -70, 70, 26, 40, 2.5)
    b.boxr("MI_Chrome", 230, 234, -40, 40, 52, 60, 1.0)
    b.colbox(-235, 235, -Wd / 2, Wd / 2, 0, 140)
    return b.save(recenter=False)


def mk_tree_round():
    b = Builder("SM_Tree_Round", "Exterior")
    b.cone("MI_TreeBark", (0, 0, 0), 16, 9, 260, 10)
    for a, l, t in ((30, 120, 40), (160, 110, 45), (270, 130, 35)):
        b.cyl("MI_TreeBark", (0, 0, 220), 5, l, 8, rot=(-t, a, 0))
    rnd = random.Random(71)
    b.sphere("MI_Leaves", (0, 0, 420), 170, (1.0, 1.0, 0.9), 7)
    for k in range(7):
        a = rnd.uniform(0, 360)
        r = rnd.uniform(90, 150)
        b.sphere("MI_Leaves", (math.cos(math.radians(a)) * r, math.sin(math.radians(a)) * r, rnd.uniform(330, 470)),
                 rnd.uniform(80, 120), (1.0, 1.0, 0.85), 6)
    b.colbox(-20, 20, -20, 20, 0, 300)
    return b.save()


def mk_tree_cone():
    b = Builder("SM_Tree_Cone", "Exterior")
    b.cyl("MI_TreeBark", (0, 0, 0), 14, 120, 10)
    for i, (r, z, h) in enumerate(((190, 90, 260), (150, 250, 230), (105, 400, 200), (60, 540, 160))):
        b.cone("MI_Leaves", (0, 0, z), r, r * 0.25, h, 14)
    b.colbox(-20, 20, -20, 20, 0, 200)
    return b.save()


def mk_lamp_post():
    b = Builder("SM_LampPost", "Exterior")
    b.cyl("MI_Concrete", (0, 0, 0), 22, 60, 16, 2.0)
    b.cone("MI_MetalDark", (0, 0, 60), 9, 6, 620, 12)
    path = [(0, 0, 670), (40, 0, 700), (110, 0, 710), (150, 0, 705)]
    b.sweep("MI_MetalDark", [(-4, -4), (4, -4), (4, 4), (-4, 4)], path)
    b.boxr("MI_MetalDark", 120, 190, -22, 22, 690, 712, 3.0)
    b.boxr("MI_LampHead", 125, 185, -18, 18, 688, 690, 0.0)
    b.colbox(-20, 20, -20, 20, 0, 680)
    return b.save()


def mk_hedge():
    b = Builder("SM_Hedge_400", "Exterior")
    b.boxr("MI_Plant", -50, 50, -200, 200, 0, 90, 18.0, col=True)
    rnd = random.Random(81)
    for k in range(10):
        b.sphere("MI_Plant", (rnd.uniform(-30, 30), -180 + k * 40, rnd.uniform(70, 90)), rnd.uniform(25, 35), (1, 1, 0.7), 5)
    return b.save()


def mk_building(name, w, d, h):
    b = Builder(name, "Exterior")
    b.mid("MI_Facade")
    b.boxr("MI_Facade", -d / 2, d / 2, -w / 2, w / 2, 0, h, 15.0)
    b.boxr("MI_MetalDark", -d / 2 - 20, d / 2 + 20, -w / 2 - 20, w / 2 + 20, h, h + 60, 10.0)
    rnd = random.Random(len(name))
    for k in range(4):
        b.boxr("MI_MetalGray", rnd.uniform(-d / 3, d / 3), 0, rnd.uniform(-w / 3, w / 3), 0, h + 60, h + 60 + rnd.uniform(100, 220), 8.0)
    return b.save()


def mk_parking_lines():
    b = Builder("SM_ParkingLines_14", "Exterior")
    for i in range(15):
        y = -7 * 260 + i * 260
        b.boxr("MI_LineWhite", -250, 250, y - 6, y + 6, 0, 0.6, 0.0)
    return b.save()


# =============================================================================================
# Architecture (world space)
# =============================================================================================

def _wall_pieces(start, end, openings):
    """Split [start, end] by openings -> solid spans + opening spans."""
    solids = []
    cur = start
    for o0, o1 in sorted(openings):
        if o0 > cur:
            solids.append((cur, o0))
        cur = max(cur, o1)
    if cur < end:
        solids.append((cur, end))
    return solids


def _span_box(b, axis, fixed, a0, a1, thick, z0, z1, mat, col=True, world_uv=True, bevel=0.0):
    if axis == "x":
        b.boxr(mat, a0, a1, fixed - thick / 2, fixed + thick / 2, z0, z1, bevel, col=col, world_uv=world_uv)
    else:
        b.boxr(mat, fixed - thick / 2, fixed + thick / 2, a0, a1, z0, z1, bevel, col=col, world_uv=world_uv)


def mk_interior_wall(wid, axis, fixed, start, end, openings, mat, kind):
    b = Builder("SM_Wall_" + wid, "Architecture/Walls")
    t = L.INT_T
    if kind == "glass":
        for s0, s1 in _wall_pieces(start, end, openings):
            # aluminium storefront: posts every ~120, rails, glass panes
            _span_box(b, axis, fixed, s0, s1, 6, 0, 8, "MI_Aluminum", col=False)
            _span_box(b, axis, fixed, s0, s1, 6, L.CEILING_Z - 6, L.WALL_H, "MI_Aluminum", col=False)
            _span_box(b, axis, fixed, s0, s1, 1.2, 8, L.CEILING_Z - 6, "MI_Glass", col=True)
            n = max(1, int(round((s1 - s0) / 120.0)))
            for i in range(n + 1):
                p = s0 + (s1 - s0) * i / n
                _span_box(b, axis, fixed, max(s0, p - 2.5), min(s1, p + 2.5), 7, 0, L.CEILING_Z, "MI_Aluminum", col=False)
            _span_box(b, axis, fixed, s0, s1, 1.6, 100, 104, "MI_AccentRed", col=False)
        for o0, o1 in openings:
            _span_box(b, axis, fixed, o0, o1, 8, L.DOOR_H, L.WALL_H, "MI_Aluminum")
        return b.save()
    for s0, s1 in _wall_pieces(start, end, openings):
        _span_box(b, axis, fixed, s0, s1, t, 0, L.WALL_H, mat)
        for side in (-1, 1):
            _span_box(b, axis, fixed + side * (t / 2 + 0.7), s0, s1, 1.4, 0, 10, "MI_Baseboard", col=False, world_uv=False)
    for o0, o1 in openings:
        _span_box(b, axis, fixed, o0, o1, t, L.DOOR_H, L.WALL_H, mat)
    return b.save()


def mk_exterior_wall(wid, axis, fixed, start, end, outward, window_spans, extra):
    """Exterior wall: pillars between window modules + header; window modules placed separately by the level."""
    b = Builder("SM_ExtWall_" + wid, "Architecture/Walls")
    T = L.EXT_T
    c = fixed + outward * T / 2.0
    openings = []
    for s0, s1 in window_spans:
        for p in L.window_positions(s0, s1):
            openings.append((p - L.WINDOW_W / 2, p + L.WINDOW_W / 2))
    for o0, o1, kind in extra:
        mid = (o0 + o1) / 2
        openings.append((mid - L.WINDOW_W / 2, mid + L.WINDOW_W / 2))
    lo, hi = start - T, end + T
    skin = fixed + outward * (T + 0.6)
    for s0, s1 in _wall_pieces(lo, hi, openings):
        _span_box(b, axis, c, s0, s1, T, -15, L.EXT_WALL_H, "MI_WallPaint")
        _span_box(b, axis, skin, s0, s1, 1.2, -15, L.EXT_WALL_H + 30, "MI_ExteriorWall", col=False)
        s0c, s1c = max(s0, start), min(s1, end)
        if s1c > s0c:
            _span_box(b, axis, fixed + (-outward) * 0.7, s0c, s1c, 1.4, 0, 10, "MI_Baseboard", col=False, world_uv=False)
    for o0, o1 in openings:
        _span_box(b, axis, c, o0, o1, T, L.WINDOW_H, L.EXT_WALL_H, "MI_WallPaint")
        _span_box(b, axis, skin, o0, o1, 1.2, L.WINDOW_H, L.EXT_WALL_H + 30, "MI_ExteriorWall", col=False)
        _span_box(b, axis, c, o0, o1, T, -15, 0, "MI_Concrete", col=False)
    return b.save(), openings


def mk_floor(name, rect, mat):
    b = Builder("SM_Floor_" + name, "Architecture/Floors")
    x0, x1, y0, y1 = rect
    b.boxr(mat, x0, x1, y0, y1, -10, 0, 0.0, col=True, world_uv=True)
    return b.save()


def mk_ceiling():
    b = Builder("SM_Ceiling", "Architecture")
    x0, x1, y0, y1 = L.BUILDING
    b.boxr("MI_CeilingTile", x0, x1, y0, y1, L.CEILING_Z, L.CEILING_Z + 3, 0.0, col=True, world_uv=True)
    return b.save()


def mk_roof():
    b = Builder("SM_Roof", "Architecture")
    x0, x1, y0, y1 = L.BUILDING
    b.boxr("MI_MetalDark", x0 - 60, x1 + 60, y0 - 60, y1 + 60, L.ROOF_Z, L.ROOF_Z + 30, 0.0, col=True, world_uv=True)
    b.boxr("MI_ExteriorWall", x0 - 60, x1 + 60, y0 - 60, y1 + 60, L.ROOF_Z + 30, L.ROOF_Z + 60, 0.0, col=False, world_uv=True)
    return b.save()


def mk_accent_panels():
    """Paint accent panels on wall faces (lobby sign wall in severance red)."""
    b = Builder("SM_AccentPanels", "Architecture")
    yface = 1800 - L.INT_T / 2
    b.boxr("MI_WallAccent_Red", 40, 500, yface - 0.6, yface, 10, L.CEILING_Z, 0.0, col=False, world_uv=True)
    return b.save()


def mk_ground():
    b = Builder("SM_Ground", "Exterior")
    # building pad + sidewalks (concrete), lots (asphalt), verges (grass)
    b.boxr("MI_Concrete", -400, 4400, -420, 0, -30, 0, 0.0, col=True, world_uv=True)
    b.boxr("MI_Concrete", 4000, 4420, -420, 3200, -30, 0, 0.0, col=True, world_uv=True)
    b.boxr("MI_Concrete", -400, 4420, 2800, 3200, -30, 0, 0.0, col=True, world_uv=True)
    b.boxr("MI_Concrete", -400, 0, -420, 3200, -30, 0, 0.0, col=True, world_uv=True)
    b.boxr("MI_Grass", -3000, 4420, -760, -420, -30, -2, 0.0, col=True, world_uv=True)
    b.boxr("MI_Grass", 4420, 4780, -760, 3200, -30, -2, 0.0, col=True, world_uv=True)
    b.boxr("MI_Asphalt", -6000, 7000, -2700, -760, -40, -12, 0.0, col=True, world_uv=True)
    b.boxr("MI_Asphalt", 4780, 7000, -760, 5000, -40, -12, 0.0, col=True, world_uv=True)
    b.boxr("MI_Grass", -9000, 14000, -9000, -2700, -42, -14, 0.0, col=True, world_uv=True)
    b.boxr("MI_Grass", 7000, 14000, -2700, 9000, -42, -14, 0.0, col=True, world_uv=True)
    b.boxr("MI_Grass", -9000, 4780, 3200, 9000, -42, -14, 0.0, col=True, world_uv=True)
    b.boxr("MI_Grass", -9000, -400, -2700, 3200, -42, -14, 0.0, col=True, world_uv=True)
    # road beyond the lots
    b.boxr("MI_Asphalt", -9000, 14000, -3900, -3000, -40, -13, 0.0, col=False, world_uv=True)
    b.boxr("MI_Asphalt", 7900, 8800, -9000, 9000, -40, -13, 0.0, col=False, world_uv=True)
    # curbs
    b.boxr("MI_Concrete", -3000, 4420, -775, -760, -14, 0, 0.0, col=False, world_uv=True)
    b.boxr("MI_Concrete", 4780, 4795, -760, 5000, -14, 0, 0.0, col=False, world_uv=True)
    # parking stripes (south rows: stalls 260 wide, 500 deep; east rows)
    for row_y0 in (-1260, -2420):
        for i in range(15):
            x = -300 + i * 260
            b.boxr("MI_LineWhite", x - 6, x + 6, row_y0, row_y0 + 500, -12, -11.4, 0.0, col=False)
    for col_x0 in (4990, 6150):
        for i in range(11):
            y = 100 + i * 260
            b.boxr("MI_LineWhite", col_x0, col_x0 + 500, y - 6, y + 6, -12, -11.4, 0.0, col=False)
    b.boxr("MI_LineYellow", -3000, 4400, -1840, -1828, -12, -11.4, 0.0, col=False)
    return b.save()


# =============================================================================================

FURNITURE = {
    "SM_Desk_Straight": mk_desk_straight,
    "SM_Desk_L": mk_desk_l,
    "SM_Pedestal": mk_pedestal,
    "SM_Partition_170": lambda: partition("SM_Partition_170", 170.0),
    "SM_Partition_120": lambda: partition("SM_Partition_120", 120.0),
    "SM_PartitionPost": mk_partition_post,
    "SM_OfficeChair": mk_office_chair,
    "SM_Monitor": mk_monitor,
    "SM_Keyboard": mk_keyboard,
    "SM_Mouse": mk_mouse,
    "SM_DeskPhone": mk_desk_phone,
    "SM_PencilCup": mk_pencil_cup,
    "SM_StickyPad": mk_sticky_pad,
    "SM_Plant_Small": mk_plant_small,
    "SM_Plant_Large": mk_plant_large,
    "SM_Plant_Bush": mk_plant_bush,
    "SM_WallClock": mk_wall_clock,
    "SM_PosterFrame": mk_poster_frame,
    "SM_FilingCabinet": mk_filing_cabinet,
    "SM_Printer_Floor": mk_printer_floor,
    "SM_Printer_Desktop": mk_printer_desktop,
    "SM_InboxTray": mk_inbox_tray,
    "SM_SecurityCamera": mk_security_camera,
    "SM_BadgeReader": mk_badge_reader,
    "SM_SecurityMonitor": mk_security_monitor,
    "SM_InventoryClipboard": mk_inventory_clipboard,
    "SM_SupplyShelf": mk_supply_shelf,
    "SM_Credenza": mk_credenza,
    "SM_Bookshelf": mk_bookshelf,
    "SM_ManagerDesk": mk_manager_desk,
    "SM_Sofa": mk_sofa,
    "SM_MeetingTable": mk_meeting_table,
    "SM_Whiteboard": mk_whiteboard,
    "SM_Rug": mk_rug,
    "SM_CoffeeTableLow": mk_coffee_table_low,
    "SM_ReceptionCounter": mk_reception_counter,
    "SM_CompanySignBoard": mk_company_sign,
    "SM_CoffeeCounter_240": mk_coffee_counter,
    "SM_CopyCounter_300": mk_copy_counter,
    "SM_Kitchen_Base60": mk_kitchen_base,
    "SM_Kitchen_Upper60": mk_kitchen_upper,
    "SM_Kitchen_MicrowaveNiche120": mk_microwave_niche,
    "SM_Kitchen_Shelf180": mk_kitchen_shelf,
    "SM_Countertop_Sink_900": mk_countertop_sink,
    "SM_Sink_Double": mk_sink_double,
    "SM_DishRack": mk_dish_rack,
    "SM_Microwave": mk_microwave,
    "SM_Fridge": mk_fridge,
    "SM_CoffeeBrewer": mk_coffee_brewer,
    "SM_WaterCooler": mk_water_cooler,
    "SM_VendingMachine": mk_vending,
    "SM_TrashBin": mk_trash_bin,
    "SM_RecycleBin": mk_recycle_bin,
    "SM_CafeTable": mk_cafe_table,
    "SM_PlasticChair": mk_plastic_chair,
    "SM_Mug": mk_mug,
    "SM_CeilingLightPanel": mk_light_panel,
    "SM_CeilingVent": mk_ceiling_vent,
    "SM_WindowModule": mk_window_module,
    "SM_EntranceDoors": mk_entrance_doors,
    "SM_DoorDecor": mk_door_decor,
    "SM_Car_Sedan": mk_car,
    "SM_Tree_Round": mk_tree_round,
    "SM_Tree_Cone": mk_tree_cone,
    "SM_LampPost": mk_lamp_post,
    "SM_Hedge_400": mk_hedge,
    "SM_Building_A": lambda: mk_building("SM_Building_A", 4000, 1600, 1300),
    "SM_Building_B": lambda: mk_building("SM_Building_B", 3000, 2000, 900),
    "SM_ParkingLines_14": mk_parking_lines,
}


def door_frame_widths():
    widths = set()
    for wid, axis, fixed, start, end, openings, mat, kind in L.INTERIOR_WALLS:
        if kind == "glass":
            continue
        for o0, o1 in openings:
            widths.add(int(round(o1 - o0)))
    return sorted(widths)


def build_architecture():
    for w in L.INTERIOR_WALLS:
        mk_interior_wall(*w)
    for w in L.EXTERIOR_WALLS:
        mk_exterior_wall(*w)
    for name, rect, mat in L.FLOORS:
        mk_floor(name, rect, mat)
    mk_ceiling()
    mk_roof()
    mk_accent_panels()
    mk_ground()
    for w in door_frame_widths():
        mk_door_frame(w)


def build(only=None):
    import time
    names = list(FURNITURE.keys())
    if only:
        names = [n for n in names if n in only]
    for n in names:
        t = time.time()
        try:
            FURNITURE[n]()
            C.log("mesh %s (%.1fs)" % (n, time.time() - t))
        except Exception:
            import traceback
            C.warn("mesh %s failed:\n%s" % (n, traceback.format_exc()))
    if not only or "architecture" in only:
        build_architecture()
        C.log("architecture meshes done")
