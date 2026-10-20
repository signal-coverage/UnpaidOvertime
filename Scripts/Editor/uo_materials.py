"""Master materials (built node-by-node with MaterialEditingLibrary) and all material instances.

The project runs with Substrate enabled, so masters feed a 'Substrate Shading Models' node into the
Front Material pin (legacy pins are ignored once Substrate is on). If that node class is missing the
builder falls back to the legacy material pins.
"""
import unreal

import uo_common as C

MEL = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty

# Lighting pass knob: self-glow of the ceiling tiles (emissive strength, same units as the light panels' 70)
CEILING_GLOW = 14.0

WHITE_TEX = "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"
FLAT_NORMAL = "/Engine/EngineMaterials/DefaultNormal.DefaultNormal"

_input_name_cache = {}


def _norm(s):
    return s.replace(" ", "").replace("_", "").lower()


def _resolve_input(expr, wanted):
    """Find the real input pin name on expr matching `wanted` (ignores spaces/case)."""
    key = (expr.get_class().get_name(), wanted)
    if key in _input_name_cache:
        return _input_name_cache[key]
    names = [str(n) for n in MEL.get_material_expression_input_names(expr)]
    for n in names:
        if _norm(n) == _norm(wanted):
            _input_name_cache[key] = n
            return n
    C.warn("input '%s' not found on %s; available %s" % (wanted, key[0], names))
    _input_name_cache[key] = wanted
    return wanted


class Graph:
    def __init__(self, material):
        self.m = material
        self.x = -1600
        self.y = 0

    def node(self, cls, **props):
        e = MEL.create_material_expression(self.m, cls, self.x, self.y)
        self.y += 140
        if self.y > 1400:
            self.y = 0
            self.x += 320
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def scalar(self, name, default, group="Surface"):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=default, group=group)

    def vector(self, name, default, group="Surface"):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=default, group=group)

    def texture(self, name, default_path, normal=False, group="Textures"):
        e = self.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=name, group=group)
        e.set_editor_property("texture", C.load(default_path))
        if normal:
            e.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        return e

    def const3(self, r, g, b):
        return self.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1))

    def const(self, v):
        return self.node(unreal.MaterialExpressionConstant, r=v)

    def op(self, cls, a, b, a_out="", b_out=""):
        e = self.node(cls)
        self.link(a, a_out, e, "A")
        self.link(b, b_out, e, "B")
        return e

    def mul(self, a, b, a_out="", b_out=""):
        return self.op(unreal.MaterialExpressionMultiply, a, b, a_out, b_out)

    def add(self, a, b, a_out="", b_out=""):
        return self.op(unreal.MaterialExpressionAdd, a, b, a_out, b_out)

    def lerp(self, a, b, alpha, a_out="", b_out="", alpha_out=""):
        e = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, a_out, e, "A")
        self.link(b, b_out, e, "B")
        self.link(alpha, alpha_out, e, "Alpha")
        return e

    def link(self, src, src_out, dst, dst_in):
        ok = MEL.connect_material_expressions(src, src_out, dst, dst_in)
        if not ok:
            C.warn("link failed %s.%s -> %s.%s" % (src.get_class().get_name(), src_out, dst.get_class().get_name(), dst_in))

    def outputs(self, pins):
        """pins: dict property-name -> (expr, out_name). Uses Substrate if available."""
        shading_cls = getattr(unreal, "MaterialExpressionSubstrateShadingModels", None)
        if shading_cls is not None:
            sm = MEL.create_material_expression(self.m, shading_cls, 400, 0)
            for prop, (expr, out) in pins.items():
                self.link(expr, out, sm, _resolve_input(sm, prop))
            if not MEL.connect_material_property(sm, "", MP.MP_FRONT_MATERIAL):
                C.warn("could not connect FrontMaterial on " + self.m.get_name())
            return sm
        legacy = {"BaseColor": MP.MP_BASE_COLOR, "Metallic": MP.MP_METALLIC, "Specular": MP.MP_SPECULAR,
                  "Roughness": MP.MP_ROUGHNESS, "Normal": MP.MP_NORMAL, "EmissiveColor": MP.MP_EMISSIVE_COLOR,
                  "Opacity": MP.MP_OPACITY}
        for prop, (expr, out) in pins.items():
            MEL.connect_material_property(expr, out, legacy[prop])
        return None


def _new_master(path):
    mat, _ = C.get_or_create(path, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    return mat


def _uv(g):
    tc = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    tiling = g.scalar("Tiling", 1.0, "UV")
    return g.mul(tc, tiling)


# Runtime office lighting level (1 = day, lower in overtime). Set by AUOGameMode through this collection.
MPC_PATH = C.MAT_DIR + "/MPC_UO_Office"
LIGHT_LEVEL = "LightLevel"


def build_mpc():
    mpc, _ = C.get_or_create(MPC_PATH, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    params = list(mpc.get_editor_property("scalar_parameters"))
    if not any(str(p.get_editor_property("parameter_name")) == LIGHT_LEVEL for p in params):
        # Only add once: the parameter GUID must stay stable for materials that reference it
        p = unreal.CollectionScalarParameter()
        p.set_editor_property("parameter_name", LIGHT_LEVEL)
        p.set_editor_property("default_value", 1.0)
        params.append(p)
        mpc.set_editor_property("scalar_parameters", params)
    C.save(mpc)
    return mpc


def _light_scaled_emissive(g, emissive):
    """emissive * lerp(1, MPC.LightLevel, LightLevelInfluence): lets light panels and the ceiling dim in overtime."""
    ll = g.node(unreal.MaterialExpressionCollectionParameter)
    ll.set_editor_property("collection", C.load(MPC_PATH))
    ll.set_editor_property("parameter_name", LIGHT_LEVEL)
    infl = g.scalar("LightLevelInfluence", 0.0, "Emissive")
    factor = g.lerp(g.const(1.0), ll, infl)
    return g.mul(emissive, factor)


def build_surface_master():
    mat = _new_master(C.MAT_DIR + "/M_UO_Surface")
    g = Graph(mat)
    uv = _uv(g)
    tex = g.texture("BaseTexture", WHITE_TEX)
    g.link(uv, "", tex, "UVs")
    amount = g.scalar("TextureAmount", 0.0)
    white = g.const3(1, 1, 1)
    tex_l = g.lerp(white, tex, amount, b_out="RGB")
    tint = g.vector("BaseColorTint", unreal.LinearColor(0.6, 0.6, 0.6, 1))
    base = g.mul(tex_l, tint)

    rough = g.scalar("Roughness", 0.6)
    rough_var = g.scalar("RoughnessVariation", 0.0)
    half = g.const(0.5)
    centered = g.op(unreal.MaterialExpressionSubtract, tex, half, "R", "")
    rough_final = g.add(rough, g.mul(centered, rough_var))

    metallic = g.scalar("Metallic", 0.0)
    specular = g.scalar("Specular", 0.5)

    ntex = g.texture("NormalTexture", FLAT_NORMAL, normal=True)
    g.link(uv, "", ntex, "UVs")
    nstr = g.scalar("NormalStrength", 0.0)
    flat = g.const3(0, 0, 1)
    normal = g.lerp(flat, ntex, nstr, b_out="RGB")

    ecol = g.vector("EmissiveColor", unreal.LinearColor(0, 0, 0, 1), "Emissive")
    estr = g.scalar("EmissiveStrength", 0.0, "Emissive")
    emissive = _light_scaled_emissive(g, g.mul(ecol, estr))

    g.outputs({"BaseColor": (base, ""), "Metallic": (metallic, ""), "Specular": (specular, ""),
               "Roughness": (rough_final, ""), "Normal": (normal, ""), "EmissiveColor": (emissive, "")})
    MEL.recompile_material(mat)
    C.save(mat)
    return mat


def build_emissive_master():
    mat = _new_master(C.MAT_DIR + "/M_UO_Emissive")
    g = Graph(mat)
    uv = _uv(g)
    tex = g.texture("EmissiveTexture", WHITE_TEX)
    g.link(uv, "", tex, "UVs")
    ecol = g.vector("EmissiveColor", unreal.LinearColor(1, 1, 1, 1), "Emissive")
    estr = g.scalar("EmissiveStrength", 10.0, "Emissive")
    emissive = _light_scaled_emissive(g, g.mul(g.mul(tex, ecol, "RGB", ""), estr))
    base_tint = g.vector("BaseColorTint", unreal.LinearColor(0.02, 0.02, 0.02, 1))
    base_amt = g.scalar("BaseTextureAmount", 0.0)
    base = g.mul(base_tint, g.lerp(g.const3(1, 1, 1), tex, base_amt, b_out="RGB"))
    rough = g.scalar("Roughness", 0.3)
    g.outputs({"BaseColor": (base, ""), "Roughness": (rough, ""), "EmissiveColor": (emissive, "")})
    MEL.recompile_material(mat)
    C.save(mat)
    return mat


def build_glass_master():
    mat = _new_master(C.MAT_DIR + "/M_UO_Glass")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)
    try:
        mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    except Exception as exc:  # property names differ between versions; non-fatal
        C.warn("translucency lighting mode: %s" % exc)
    g = Graph(mat)
    tint = g.vector("BaseColorTint", unreal.LinearColor(0.8, 0.88, 0.92, 1))
    opacity = g.scalar("Opacity", 0.18)
    rough = g.scalar("Roughness", 0.04)
    spec = g.scalar("Specular", 0.6)
    metallic = g.scalar("Metallic", 0.0)
    g.outputs({"BaseColor": (tint, ""), "Opacity": (opacity, ""), "Roughness": (rough, ""), "Specular": (spec, ""),
               "Metallic": (metallic, "")})
    MEL.recompile_material(mat)
    C.save(mat)
    return mat


def build_outline_pp():
    """Post-process outline around primitives that render custom depth with a stencil (focused station).

    Requires r.CustomDepth=3 (DefaultEngine.ini). Added at runtime by AUOCharacter to its camera.
    Post-process materials use the legacy Emissive pin (Substrate only affects surface/decal domains).
    """
    mat = _new_master(C.MAT_DIR + "/M_UO_Outline_PP")
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    g = Graph(mat)

    uv = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    view = g.node(unreal.MaterialExpressionViewSize)
    width = g.scalar("OutlineWidth", 2.0, "Outline")
    big = g.const(1000.0)

    def stencil_mask(dx, dy):
        """saturate(stencil * 1000) sampled dx, dy outline-widths away (works for raw or normalized stencil)."""
        st = g.node(unreal.MaterialExpressionSceneTexture, scene_texture_id=unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
        if dx or dy:
            off = g.node(unreal.MaterialExpressionConstant2Vector, r=float(dx), g=float(dy))
            delta = g.op(unreal.MaterialExpressionDivide, g.mul(off, width), view)
            g.link(g.add(uv, delta), "", st, "UVs")
        else:
            g.link(uv, "", st, "UVs")
        r = g.node(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
        g.link(st, "Color", r, "")
        sat = g.node(unreal.MaterialExpressionSaturate)
        g.link(g.mul(r, big), "", sat, "")
        return sat

    center = stencil_mask(0, 0)
    total = None
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (0.7, 0.7), (-0.7, 0.7), (0.7, -0.7), (-0.7, -0.7)):
        s = stencil_mask(dx, dy)
        total = s if total is None else g.add(total, s)
    neighbors = g.node(unreal.MaterialExpressionSaturate)
    g.link(total, "", neighbors, "")
    inside = g.node(unreal.MaterialExpressionOneMinus)
    g.link(center, "", inside, "")
    edge = g.mul(neighbors, inside)

    scene = g.node(unreal.MaterialExpressionSceneTexture, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    g.link(uv, "", scene, "UVs")
    rgb = g.node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
    g.link(scene, "Color", rgb, "")

    fill_col = g.vector("FillColor", unreal.LinearColor(1.0, 0.96, 0.88, 1), "Outline")
    fill_amt = g.scalar("FillAmount", 0.07, "Outline")
    filled = g.lerp(rgb, fill_col, g.mul(center, fill_amt))
    out_col = g.vector("OutlineColor", unreal.LinearColor(1.0, 0.94, 0.78, 1), "Outline")
    out_op = g.scalar("OutlineOpacity", 0.95, "Outline")
    result = g.lerp(filled, out_col, g.mul(edge, out_op))

    # Substrate: post-process materials output through a Substrate Post Process node on Front Material
    pp_cls = getattr(unreal, "MaterialExpressionSubstratePostProcess", None)
    if pp_cls is not None:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        pp = MEL.create_material_expression(mat, pp_cls, 400, 0)
        g.link(result, "", pp, _resolve_input(pp, "Color"))
        if not MEL.connect_material_property(pp, "", MP.MP_FRONT_MATERIAL):
            C.warn("outline: could not connect FrontMaterial")
    elif not MEL.connect_material_property(result, "", MP.MP_EMISSIVE_COLOR):
        C.warn("outline: could not connect EmissiveColor")
    MEL.recompile_material(mat)
    C.save(mat)
    return mat


def build_sign_text():
    """Lit, masked text material for TextRender components (company sign): dark lettering on the aluminium board."""
    mat = _new_master(C.MAT_DIR + "/M_UO_SignText")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    g = Graph(mat)
    font = g.node(unreal.MaterialExpressionFontSampleParameter, parameter_name="Font")
    font.set_editor_property("font", C.load("/Engine/EngineFonts/RobotoDistanceField"))
    font.set_editor_property("font_texture_page", 0)
    color = g.vector("TextColor", C.srgb("1b1c1f"), "Text")
    rough = g.scalar("Roughness", 0.45, "Text")
    g.outputs({"BaseColor": (color, ""), "Roughness": (rough, "")})
    if not MEL.connect_material_property(font, "A", MP.MP_OPACITY_MASK):
        C.warn("sign text: could not connect OpacityMask")
    MEL.recompile_material(mat)
    C.save(mat)
    return mat


# --------------------------------------------------------------------------------------------
# Instances
# --------------------------------------------------------------------------------------------

S = C.srgb
T = C.TEX_DIR + "/"

# name: (master, params). params keys: tint, tex, amount, tiling, rough, rvar, metal, spec, ntex, nstr, ecol, estr, opacity
INSTANCES = {
    # Floors / ceiling / walls
    "MI_Carpet": ("S", dict(tex="T_Carpet_D", amount=1, tint=S("ffffff"), tiling=1.0, rough=0.95, ntex="T_Carpet_N", nstr=0.5)),
    "MI_Carpet_Dark": ("S", dict(tex="T_Carpet_D", amount=1, tint=S("a7aab8"), tiling=1.0, rough=0.95, ntex="T_Carpet_N", nstr=0.5)),
    # The ceiling only sees bounce light from the (warm) carpet; a faint cool self-glow keeps it reading as a
    # clean bright fluorescent ceiling instead of brown. Dims with the office lights in overtime (infl=1).
    "MI_CeilingTile": ("S", dict(tex="T_CeilingTile_D", amount=1, tint=C.lc(1.1, 1.1, 1.12), tiling=1 / 0.6, rough=0.92, ntex="T_CeilingTile_N", nstr=0.6,
                                 ecol=C.lc(0.93, 0.96, 1.0), estr=CEILING_GLOW, infl=1.0)),
    "MI_KitchenTile": ("S", dict(tex="T_KitchenTile_D", amount=1, tint=S("ffffff"), tiling=1 / 0.6, rough=0.35, rvar=0.2, ntex="T_KitchenTile_N", nstr=0.7)),
    "MI_LobbyTile": ("S", dict(tex="T_KitchenTile_D", amount=1, tint=S("d8d0c4"), tiling=1 / 1.2, rough=0.22, rvar=0.1, ntex="T_KitchenTile_N", nstr=0.5)),
    "MI_WallPaint": ("S", dict(tex="T_WallPaint_D", amount=1, tint=S("e2e0da"), tiling=0.5, rough=0.85)),
    "MI_WallWarm": ("S", dict(tex="T_WallPaint_D", amount=1, tint=S("e6dccb"), tiling=0.5, rough=0.85)),
    "MI_WallAccent_Red": ("S", dict(tex="T_WallPaint_D", amount=1, tint=S("8e2026"), tiling=0.5, rough=0.8)),
    "MI_WallAccent_Green": ("S", dict(tex="T_WallPaint_D", amount=1, tint=S("a9b46e"), tiling=0.5, rough=0.8)),
    "MI_Baseboard": ("S", dict(tint=S("4a4a4c"), rough=0.55)),
    "MI_ExteriorWall": ("S", dict(tex="T_Concrete_D", amount=1, tint=S("e8e4dc"), tiling=0.5, rough=0.9)),
    # Furniture
    "MI_WoodTop": ("S", dict(tex="T_Wood_D", amount=1, tint=S("ffffff"), tiling=1.0, rough=0.42, rvar=0.25)),
    "MI_WoodDark": ("S", dict(tex="T_Wood_D", amount=1, tint=S("8a5a3c"), tiling=1.0, rough=0.38, rvar=0.25)),
    "MI_DoorWood": ("S", dict(tex="T_Wood_D", amount=1, tint=S("d8c0a0"), tiling=1.0, rough=0.5)),
    "MI_MetalGray": ("S", dict(tex="T_BrushedMetal_D", amount=0.6, tint=S("a6a9ad"), tiling=2, rough=0.42)),
    "MI_MetalDark": ("S", dict(tex="T_BrushedMetal_D", amount=0.5, tint=S("4b4e53"), tiling=2, rough=0.4)),
    "MI_Aluminum": ("S", dict(tex="T_BrushedMetal_D", amount=1, tint=S("c4c6c9"), tiling=2, rough=0.35, metal=1.0)),
    "MI_Stainless": ("S", dict(tex="T_BrushedMetal_D", amount=1, tint=S("d6d8da"), tiling=2, rough=0.26, metal=1.0)),
    "MI_Chrome": ("S", dict(tint=S("e8e8e8"), rough=0.12, metal=1.0)),
    "MI_Fabric": ("S", dict(tex="T_Fabric_D", amount=1, tint=S("c2c3c4"), tiling=4, rough=0.92, ntex="T_Fabric_N", nstr=0.6)),
    "MI_FabricBlue": ("S", dict(tex="T_Fabric_D", amount=1, tint=S("5f8ea0"), tiling=4, rough=0.92, ntex="T_Fabric_N", nstr=0.6)),
    "MI_FabricRed": ("S", dict(tex="T_Fabric_D", amount=1, tint=S("b03a3a"), tiling=4, rough=0.92, ntex="T_Fabric_N", nstr=0.6)),
    "MI_ChairMesh": ("S", dict(tex="T_Fabric_D", amount=1, tint=S("3a3a3d"), tiling=10, rough=0.75, ntex="T_Fabric_N", nstr=0.8)),
    "MI_PlasticBlack": ("S", dict(tint=S("1f1f21"), rough=0.42)),
    "MI_PlasticWhite": ("S", dict(tint=S("e4e3df"), rough=0.4)),
    "MI_PlasticGray": ("S", dict(tint=S("7c7f84"), rough=0.45)),
    "MI_PlasticBeige": ("S", dict(tint=S("d8d0bd"), rough=0.45)),
    "MI_Cabinet_Cream": ("S", dict(tint=S("ebe3cc"), rough=0.5)),
    "MI_Countertop": ("S", dict(tex="T_Concrete_D", amount=0.35, tint=S("b9b8b4"), tiling=0.25, rough=0.3)),
    "MI_Paper": ("S", dict(tint=S("f5f5f0"), rough=0.85)),
    "MI_Cardboard": ("S", dict(tint=S("b08655"), rough=0.9)),
    "MI_Rubber": ("S", dict(tint=S("1a1a1a"), rough=0.85)),
    "MI_Coffee": ("S", dict(tint=S("2a160b"), rough=0.08, spec=0.6)),
    "MI_Books_A": ("S", dict(tint=S("2f4f7a"), rough=0.7)),
    "MI_Books_B": ("S", dict(tint=S("8a3a2e"), rough=0.7)),
    "MI_Books_C": ("S", dict(tint=S("d1b25a"), rough=0.7)),
    "MI_ClockFace": ("S", dict(tex="T_ClockFace_D", amount=1, tint=S("ffffff"), tiling=1, rough=0.3)),
    "MI_Sign_Men": ("S", dict(tex="T_Sign_Men_D", amount=1, tint=S("ffffff"), tiling=1, rough=0.35)),
    "MI_Sign_Women": ("S", dict(tex="T_Sign_Women_D", amount=1, tint=S("ffffff"), tiling=1, rough=0.35)),
    "MI_Sign_Server": ("S", dict(tex="T_Sign_Server_D", amount=1, tint=S("ffffff"), tiling=1, rough=0.35)),
    "MI_Blinds": ("S", dict(tex="T_Blinds_D", amount=1, tint=S("f0efe9"), tiling=2, rough=0.5)),
    "MI_StickyYellow": ("S", dict(tint=S("f2e15a"), rough=0.8)),
    "MI_StickyPink": ("S", dict(tint=S("f28cb1"), rough=0.8)),
    # Plants
    "MI_Plant": ("S", dict(tex="T_Leaves_D", amount=1, tint=S("ffffff"), tiling=2, rough=0.6)),
    "MI_Succulent": ("S", dict(tex="T_Leaves_D", amount=0.5, tint=S("9cc49a"), tiling=3, rough=0.55)),
    "MI_Pot": ("S", dict(tint=S("b8673f"), rough=0.75)),
    "MI_PotWhite": ("S", dict(tint=S("e9e7e1"), rough=0.35)),
    "MI_Soil": ("S", dict(tint=S("3b2c20"), rough=0.95)),
    # Exterior
    "MI_Asphalt": ("S", dict(tex="T_Asphalt_D", amount=1, tint=S("ffffff"), tiling=0.25, rough=0.85)),
    "MI_Concrete": ("S", dict(tex="T_Concrete_D", amount=1, tint=S("ffffff"), tiling=1 / 3.0, rough=0.85)),
    "MI_Grass": ("S", dict(tex="T_Grass_D", amount=1, tint=S("ffffff"), tiling=0.25, rough=0.95)),
    "MI_LineWhite": ("S", dict(tint=S("e8e6dc"), rough=0.7)),
    "MI_LineYellow": ("S", dict(tint=S("e0b93a"), rough=0.7)),
    "MI_Facade": ("S", dict(tex="T_Facade_D", amount=1, tint=S("ffffff"), tiling=1 / 12.0, rough=0.6)),
    "MI_FacadeAlt": ("S", dict(tex="T_Facade_D", amount=1, tint=S("c9d2d8"), tiling=1 / 12.0, rough=0.6)),
    "MI_Leaves": ("S", dict(tex="T_Leaves_D", amount=1, tint=S("ffffff"), tiling=1, rough=0.7)),
    "MI_TreeBark": ("S", dict(tint=S("6e5440"), rough=0.9)),
    "MI_CarPaint_Red": ("S", dict(tint=S("9e1c22"), rough=0.25, metal=0.3)),
    "MI_CarPaint_Blue": ("S", dict(tint=S("2b4f7e"), rough=0.25, metal=0.3)),
    "MI_CarPaint_White": ("S", dict(tint=S("e6e6e3"), rough=0.25, metal=0.1)),
    "MI_CarPaint_Silver": ("S", dict(tint=S("a9adb2"), rough=0.25, metal=0.6)),
    "MI_CarPaint_Green": ("S", dict(tint=S("7c8a4a"), rough=0.3, metal=0.3)),
    "MI_CarPaint_Black": ("S", dict(tint=S("1d1e20"), rough=0.22, metal=0.3)),
    # Accents (horror palette)
    "MI_AccentRed": ("S", dict(tint=S("a51c24"), rough=0.4)),
    "MI_AccentGreen": ("S", dict(tint=S("9fb84a"), rough=0.45)),
    "MI_AccentYellow": ("S", dict(tint=S("e6b830"), rough=0.45)),
    "MI_AccentBlue": ("S", dict(tint=S("2f6fb0"), rough=0.45)),
    "MI_Mug_Red": ("S", dict(tint=S("b22228"), rough=0.2)),
    "MI_Mug_Yellow": ("S", dict(tint=S("e9c23a"), rough=0.2)),
    "MI_Mug_Blue": ("S", dict(tint=S("2f6fb0"), rough=0.2)),
    "MI_Mug_White": ("S", dict(tint=S("f1efe9"), rough=0.2)),
    "MI_Mug_Green": ("S", dict(tint=S("8fb04a"), rough=0.2)),
    "MI_Recycle": ("S", dict(tint=S("2f6fb0"), rough=0.5)),
    # Emissive
    "MI_LightPanel": ("E", dict(ecol=C.lc(0.97, 0.98, 1.0), estr=70.0, tint=S("f4f4f4"), rough=0.6, infl=1.0)),
    "MI_ScreenOn": ("E", dict(tex="T_Screen_D", ecol=C.lc(1, 1, 1), estr=18.0, tiling=1.0, rough=0.25)),
    "MI_PrinterScreen": ("E", dict(ecol=C.srgb("6fb6e8"), estr=3.0, rough=0.25)),
    "MI_VendingGlow": ("E", dict(ecol=C.lc(0.9, 0.95, 1.0), estr=6.0, tint=S("dfe6ee"), rough=0.5)),
    "MI_LampHead": ("E", dict(ecol=C.lc(1.0, 0.85, 0.6), estr=4.0, tint=S("f0e6d0"), rough=0.5)),
    "MI_LedRed": ("E", dict(ecol=C.lc(1.0, 0.05, 0.03), estr=8.0, rough=0.3)),
    "MI_LedGreen": ("E", dict(ecol=C.lc(0.45, 1.0, 0.1), estr=8.0, rough=0.3)),
    "MI_CarLightFront": ("E", dict(ecol=C.lc(1.0, 0.95, 0.85), estr=0.5, tint=S("e8e8e8"), rough=0.2)),
    "MI_CarLightRear": ("E", dict(ecol=C.lc(0.8, 0.05, 0.03), estr=0.5, tint=S("8a1010"), rough=0.2)),
    # Glass
    "MI_Glass": ("G", dict(tint=C.lc(0.75, 0.85, 0.9), opacity=0.12, rough=0.03)),
    "MI_GlassFrosted": ("G", dict(tint=C.lc(0.9, 0.92, 0.94), opacity=0.55, rough=0.35)),
    "MI_GlassDark": ("G", dict(tint=C.lc(0.05, 0.07, 0.09), opacity=0.8, rough=0.05)),
    "MI_GlassCarafe": ("G", dict(tint=C.lc(0.85, 0.9, 0.9), opacity=0.25, rough=0.03)),
    "MI_WaterBottle": ("G", dict(tint=C.lc(0.45, 0.7, 0.95), opacity=0.4, rough=0.05)),
}

POSTER_NAMES = ["Synergy", "Overtime", "Teamwork", "Smile", "Trust"]
for _p in POSTER_NAMES:
    INSTANCES["MI_Poster_" + _p] = ("S", dict(tex="T_Poster_" + _p, amount=1, tint=S("ffffff"), tiling=1.0, rough=0.3))


def mi_path(name):
    return C.MAT_DIR + "/" + name


def _set_params(mi, params):
    if "tint" in params:
        MEL.set_material_instance_vector_parameter_value(mi, "BaseColorTint", params["tint"])
    if "tex" in params:
        tex = C.load(C.TEX_DIR + "/" + params["tex"])
        name = "EmissiveTexture" if mi.get_editor_property("parent").get_name() == "M_UO_Emissive" else "BaseTexture"
        MEL.set_material_instance_texture_parameter_value(mi, name, tex)
        if name == "EmissiveTexture":
            MEL.set_material_instance_scalar_parameter_value(mi, "BaseTextureAmount", 1.0)
            MEL.set_material_instance_vector_parameter_value(mi, "BaseColorTint", C.lc(0.03, 0.03, 0.03))
    scalars = {"amount": "TextureAmount", "tiling": "Tiling", "rough": "Roughness", "rvar": "RoughnessVariation",
               "metal": "Metallic", "spec": "Specular", "nstr": "NormalStrength", "estr": "EmissiveStrength",
               "opacity": "Opacity", "infl": "LightLevelInfluence"}
    for key, pname in scalars.items():
        if key in params:
            MEL.set_material_instance_scalar_parameter_value(mi, pname, float(params[key]))
    if "ntex" in params:
        MEL.set_material_instance_texture_parameter_value(mi, "NormalTexture", C.load(C.TEX_DIR + "/" + params["ntex"]))
    if "ecol" in params:
        MEL.set_material_instance_vector_parameter_value(mi, "EmissiveColor", params["ecol"])


def build_instances(masters):
    factory = unreal.MaterialInstanceConstantFactoryNew()
    for name, (master_key, params) in INSTANCES.items():
        mi, _ = C.get_or_create(mi_path(name), unreal.MaterialInstanceConstant, factory)
        MEL.set_material_instance_parent(mi, masters[master_key])
        MEL.clear_all_material_instance_parameters(mi)
        _set_params(mi, params)
        MEL.update_material_instance(mi)
        C.save(mi)
    C.log("material instances: %d" % len(INSTANCES))


def build():
    build_mpc()
    masters = {"S": build_surface_master(), "E": build_emissive_master(), "G": build_glass_master()}
    build_instances(masters)
    build_outline_pp()
    build_sign_text()


def get(name):
    """Load a material instance by short name (cached)."""
    m = _cache.get(name)
    if m is None:
        m = C.load(mi_path(name))
        if m is None:
            C.warn("missing material " + name)
        _cache[name] = m
    return m


_cache = {}
