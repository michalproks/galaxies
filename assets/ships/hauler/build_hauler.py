"""Procedurally builds the "Hauler" ship model in Blender.

Usage (from the repository root):
    blender --background --factory-startup --python assets/ships/hauler/build_hauler.py -- \
        --save assets/ships/hauler/hauler.blend [--render preview.png]

Axes: +Y is forward (nose), +Z is up (dorsal), +X is starboard. 1 unit ~ 2 m.
"""

import argparse
import math
import random
import sys

import bmesh
import bpy
from mathutils import Euler, Matrix, Quaternion, Vector

# ---------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------


def _new_material(name):
    mat = bpy.data.materials.new(name)
    nt = mat.node_tree
    for node in list(nt.nodes):
        nt.nodes.remove(node)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    out.location = (600, 0)
    return mat, nt, out


def _socket(sockets, identifier):
    return next(s for s in sockets if s.identifier == identifier)


def _mix_color(nt, fac, a, b, blend="MIX"):
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = blend
    if isinstance(fac, float):
        _socket(mix.inputs, "Factor_Float").default_value = fac
    else:
        nt.links.new(fac, _socket(mix.inputs, "Factor_Float"))
    for socket_id, value in (("A_Color", a), ("B_Color", b)):
        if isinstance(value, tuple):
            _socket(mix.inputs, socket_id).default_value = value
        else:
            nt.links.new(value, _socket(mix.inputs, socket_id))
    return _socket(mix.outputs, "Result_Color")


def panel_material(name, light, dark, roughness=0.55, metallic=0.35):
    """Grey hull plating with tiled panels, panel lines and grime."""
    mat, nt, out = _new_material(name)
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (300, 0)
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])

    uv = nt.nodes.new("ShaderNodeTexCoord").outputs["UV"]

    def brick(scale, c1, c2, mortar, width, height, offset, rotate=False):
        node = nt.nodes.new("ShaderNodeTexBrick")
        node.offset = offset
        node.squash = 1.0
        node.inputs["Scale"].default_value = scale
        node.inputs["Color1"].default_value = c1
        node.inputs["Color2"].default_value = c2
        node.inputs["Mortar"].default_value = (0.02, 0.02, 0.02, 1.0)
        node.inputs["Mortar Size"].default_value = mortar
        node.inputs["Brick Width"].default_value = width
        node.inputs["Row Height"].default_value = height
        vector = uv
        if rotate:
            # Cross-grain layer breaks up the regular brick rows of the fine tiles.
            mapping = nt.nodes.new("ShaderNodeMapping")
            mapping.inputs["Rotation"].default_value = (0.0, 0.0, math.pi / 2)
            mapping.inputs["Location"].default_value = (0.37, 0.61, 0.0)
            nt.links.new(uv, mapping.inputs["Vector"])
            vector = mapping.outputs["Vector"]
        nt.links.new(vector, node.inputs["Vector"])
        return node

    dim = tuple(c * 0.62 for c in light[:3]) + (1.0,)
    small = brick(0.8, light, dim, 0.01, 1.4, 0.55, 0.3)
    big = brick(0.3, (1.0, 1.0, 1.0, 1.0), dark, 0.006, 1.7, 1.1, 0.65, rotate=True)

    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 3.0
    noise.inputs["Detail"].default_value = 8.0
    nt.links.new(uv, noise.inputs["Vector"])
    grime_ramp = nt.nodes.new("ShaderNodeValToRGB")
    grime_ramp.color_ramp.elements[0].position = 0.45
    grime_ramp.color_ramp.elements[0].color = (0.7, 0.7, 0.7, 1.0)
    grime_ramp.color_ramp.elements[1].position = 0.7
    grime_ramp.color_ramp.elements[1].color = (1.0, 1.0, 1.0, 1.0)
    nt.links.new(noise.outputs["Fac"], grime_ramp.inputs["Fac"])

    color = _mix_color(nt, 1.0, small.outputs["Color"], big.outputs["Color"], "MULTIPLY")
    color = _mix_color(nt, 1.0, color, grime_ramp.outputs["Color"], "MULTIPLY")
    nt.links.new(color, bsdf.inputs["Base Color"])

    lines = nt.nodes.new("ShaderNodeMath")
    lines.operation = "MAXIMUM"
    nt.links.new(small.outputs["Fac"], lines.inputs[0])
    nt.links.new(big.outputs["Fac"], lines.inputs[1])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.6
    bump.inputs["Distance"].default_value = 0.02
    bump.invert = True
    nt.links.new(lines.outputs["Value"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    rough = nt.nodes.new("ShaderNodeMapRange")
    rough.inputs["To Min"].default_value = roughness - 0.15
    rough.inputs["To Max"].default_value = roughness + 0.15
    nt.links.new(noise.outputs["Fac"], rough.inputs["Value"])
    nt.links.new(rough.outputs["Result"], bsdf.inputs["Roughness"])
    bsdf.inputs["Metallic"].default_value = metallic
    return mat


def flat_material(name, color, roughness=0.5, metallic=0.0, emission=None, strength=0.0):
    mat, nt, out = _new_material(name)
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if emission is not None:
        bsdf.inputs["Emission Color"].default_value = emission
        bsdf.inputs["Emission Strength"].default_value = strength
    return mat


def plume_material(name, length):
    """Additive exhaust plume: emission fading along the plume (object -Y) and towards its silhouette."""
    mat, nt, out = _new_material(name)
    coords = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(coords.outputs["Object"], sep.inputs["Vector"])
    along = nt.nodes.new("ShaderNodeMapRange")
    along.inputs["From Min"].default_value = -length
    along.inputs["From Max"].default_value = 0.0
    along.inputs["To Min"].default_value = 0.0
    along.inputs["To Max"].default_value = 1.0
    nt.links.new(sep.outputs["Y"], along.inputs["Value"])
    falloff = nt.nodes.new("ShaderNodeMath")
    falloff.operation = "POWER"
    falloff.inputs[1].default_value = 4.0
    nt.links.new(along.outputs["Result"], falloff.inputs[0])

    facing = nt.nodes.new("ShaderNodeLayerWeight")
    facing.inputs["Blend"].default_value = 0.5
    edge = nt.nodes.new("ShaderNodeMath")
    edge.operation = "SUBTRACT"
    edge.inputs[0].default_value = 1.0
    nt.links.new(facing.outputs["Facing"], edge.inputs[1])
    soft = nt.nodes.new("ShaderNodeMath")
    soft.operation = "POWER"
    soft.inputs[1].default_value = 3.0
    nt.links.new(edge.outputs["Value"], soft.inputs[0])
    fac = nt.nodes.new("ShaderNodeMath")
    fac.operation = "MULTIPLY"
    nt.links.new(falloff.outputs["Value"], fac.inputs[0])
    nt.links.new(soft.outputs["Value"], fac.inputs[1])
    strength = nt.nodes.new("ShaderNodeMath")
    strength.operation = "MULTIPLY"
    strength.inputs[1].default_value = 2.0
    nt.links.new(fac.outputs["Value"], strength.inputs[0])

    emit = nt.nodes.new("ShaderNodeEmission")
    emit.inputs["Color"].default_value = (0.2, 0.45, 1.0, 1.0)
    nt.links.new(strength.outputs["Value"], emit.inputs["Strength"])
    transparent = nt.nodes.new("ShaderNodeBsdfTransparent")
    add = nt.nodes.new("ShaderNodeAddShader")
    nt.links.new(transparent.outputs["BSDF"], add.inputs[0])
    nt.links.new(emit.outputs["Emission"], add.inputs[1])
    nt.links.new(add.outputs["Shader"], out.inputs["Surface"])

    if hasattr(mat, "surface_render_method"):
        mat.surface_render_method = "BLENDED"
    elif hasattr(mat, "blend_method"):
        mat.blend_method = "BLEND"
    return mat


MATERIALS = {}


def create_materials():
    MATERIALS.update(
        hull=panel_material("Hull_Plating", (0.42, 0.43, 0.45, 1.0), (0.6, 0.6, 0.62, 1.0)),
        dark=panel_material("Hull_Dark", (0.13, 0.135, 0.14, 1.0), (0.55, 0.55, 0.55, 1.0), 0.6, 0.5),
        metal=flat_material("Machinery", (0.08, 0.08, 0.085, 1.0), roughness=0.4, metallic=0.9),
        trim=flat_material(
            "Markings_Orange", (0.85, 0.25, 0.03, 1.0), 0.5, 0.0, (1.0, 0.3, 0.05, 1.0), 0.4
        ),
        trim_glow=flat_material(
            "Engine_Ring_Glow", (1.0, 0.35, 0.08, 1.0), 0.4, 0.0, (1.0, 0.32, 0.06, 1.0), 5.0
        ),
        window=flat_material("Window_Blue", (0.05, 0.1, 0.3, 1.0), 0.1, 0.0, (0.2, 0.4, 1.0, 1.0), 2.0),
        engine_glow=flat_material(
            "Engine_Core", (0.6, 0.8, 1.0, 1.0), 0.2, 0.0, (0.55, 0.75, 1.0, 1.0), 40.0
        ),
        plume=plume_material("Engine_Plume", PLUME_LENGTH),
    )


# ---------------------------------------------------------------------------
# Geometry
# ---------------------------------------------------------------------------


def profile(cx, cz, w, h, ct, cb=None):
    """Chamfered rectangle in the XZ plane (half extents w, h; top/bottom chamfers)."""
    cb = ct if cb is None else cb
    ct = min(ct, w * 0.95, h * 0.95)
    cb = min(cb, w * 0.95, h * 0.95)
    return [
        (cx - w + ct, cz + h),
        (cx + w - ct, cz + h),
        (cx + w, cz + h - ct),
        (cx + w, cz - h + cb),
        (cx + w - cb, cz - h),
        (cx - w + cb, cz - h),
        (cx - w, cz - h + cb),
        (cx - w, cz + h - ct),
    ]


class MeshBuilder:
    """Accumulates primitives into a single bmesh with per-face material slots."""

    def __init__(self, name, materials):
        self.name = name
        self.materials = materials
        self.slots = {key: i for i, key in enumerate(materials)}
        self.bm = bmesh.new()

    def _tag(self, verts, mat):
        idx = self.slots[mat]
        for v in verts:
            for f in v.link_faces:
                f.material_index = idx

    def loft(self, sections, mat):
        """sections: list of (y, cx, cz, w, h, ct, cb), ordered by increasing y."""
        bm = self.bm
        rings = []
        for y, cx, cz, w, h, ct, cb in sections:
            rings.append([bm.verts.new((x, y, z)) for x, z in profile(cx, cz, w, h, ct, cb)])
        n = len(rings[0])
        for a, b in zip(rings, rings[1:]):
            for i in range(n):
                j = (i + 1) % n
                bm.faces.new((a[i], a[j], b[j], b[i]))
        bm.faces.new(rings[0])
        bm.faces.new(list(reversed(rings[-1])))
        self._tag([v for ring in rings for v in ring], mat)

    def block(self, y0, y1, cx, cz, w, h, ct, cb=None, end=0.3, mat="hull"):
        """Chamfered box spanning y0..y1 with bevelled front and rear ends."""
        cb = ct if cb is None else cb
        e = min(end, (y1 - y0) * 0.45, w * 0.6, h * 0.6)
        shrink = (cx, cz, w - e, h - e, max(ct - 0.4 * e, 0.03), max(cb - 0.4 * e, 0.03))
        full = (cx, cz, w, h, ct, cb)
        self.loft(
            [(y0,) + shrink, (y0 + e,) + full, (y1 - e,) + full, (y1,) + shrink],
            mat,
        )

    def box(self, center, size, mat, rotation=None):
        rot = (rotation or Euler((0, 0, 0))).to_matrix().to_4x4()
        m = Matrix.Translation(center) @ rot @ Matrix.Diagonal(Vector(size).to_4d())
        ret = bmesh.ops.create_cube(self.bm, size=1.0, matrix=m)
        self._tag(ret["verts"], mat)

    def cylinder(self, center, radius, length, mat, axis="Y", radius2=None, segments=24, caps=True):
        """Cylinder/cone along an axis; `radius` is at the negative end, `radius2` at the positive."""
        rot = {
            "Y": Matrix.Rotation(-math.pi / 2, 4, "X"),
            "X": Matrix.Rotation(math.pi / 2, 4, "Y"),
            "Z": Matrix.Identity(4),
        }[axis]
        m = Matrix.Translation(center) @ rot
        ret = bmesh.ops.create_cone(
            self.bm,
            cap_ends=caps,
            cap_tris=False,
            segments=segments,
            radius1=radius,
            radius2=radius if radius2 is None else radius2,
            depth=length,
            matrix=m,
        )
        self._tag(ret["verts"], mat)

    def build(self, collection, parent, bevel=0.03):
        bm = self.bm
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        # Box-projected UVs in world units so panel textures keep a uniform density.
        uv_layer = bm.loops.layers.uv.new("UVMap")
        for face in bm.faces:
            n = face.normal
            axis = max(range(3), key=lambda k: abs(n[k]))
            for loop in face.loops:
                co = loop.vert.co
                loop[uv_layer].uv = {0: (co.y, co.z), 1: (co.x, co.z), 2: (co.x, co.y)}[axis]
        mesh = bpy.data.meshes.new(self.name)
        bm.to_mesh(mesh)
        bm.free()
        for key in self.materials:
            mesh.materials.append(MATERIALS[key])
        obj = bpy.data.objects.new(self.name, mesh)
        collection.objects.link(obj)
        obj.parent = parent
        if bevel > 0:
            mod = obj.modifiers.new("Bevel", "BEVEL")
            mod.width = bevel
            mod.segments = 1
            mod.limit_method = "ANGLE"
            mod.angle_limit = math.radians(30)
        return obj


# ---------------------------------------------------------------------------
# Ship layout
# ---------------------------------------------------------------------------

# Hull blocks: (y0, y1, cx, cz, w, h, chamfer_top, chamfer_bottom, end_bevel); w/h are half extents.
ENGINEERING = (-12.6, -7.0, 0.0, 0.0, 1.9, 1.9, 0.55, 0.55, 0.3)
MID_HULL = (-7.6, 3.6, 0.0, -0.2, 2.6, 2.2, 0.9, 0.7, 0.6)
DORSAL_HUMP = (-6.9, -0.8, 0.0, 2.0, 2.0, 1.0, 0.6, 0.2, 0.7)
PORT_POD = (-6.4, -1.4, -2.75, 0.5, 0.95, 1.4, 0.5, 0.5, 0.5)
STARBOARD_POD = (-6.4, -1.4, 2.75, 0.5, 0.95, 1.4, 0.5, 0.5, 0.5)
EMBLEM_POD = (-0.6, 4.6, 2.55, -0.6, 0.85, 1.1, 0.4, 0.4, 0.45)
BELLY = (-5.4, 2.4, 0.0, -2.55, 1.6, 0.6, 0.2, 0.4, 0.4)
DORSAL_SPINE = (1.6, 9.2, 0.0, 2.05, 1.35, 0.5, 0.35, 0.1, 0.5)
STARBOARD_BOOM = (1.0, 14.2, 2.1, -1.15, 0.55, 0.55, 0.15, 0.15, 0.25)
PORT_BOOM = (3.0, 12.6, -1.8, -1.25, 0.45, 0.45, 0.12, 0.12, 0.2)
FORWARD_HULL = [
    # (y, cx, cz, w, h, ct, cb)
    (3.0, 0.0, 0.4, 2.2, 1.9, 0.75, 0.6),
    (9.8, 0.0, 0.65, 1.75, 1.35, 0.6, 0.5),
    (11.7, 0.0, 0.8, 1.25, 0.85, 0.45, 0.35),
    (12.3, 0.0, 0.8, 0.8, 0.5, 0.25, 0.2),
]
NOZZLE_Y = -15.3
PLUME_LENGTH = 14.0


def block_faces(spec):
    """Flat face regions of a block for greeble placement: (axis, sign, plane, u-range, v-range)."""
    y0, y1, cx, cz, w, h, ct, cb, end = spec
    v = (y0 + end + 0.2, y1 - end - 0.2)
    return {
        "top": ("z", 1, cz + h, (cx - w + ct, cx + w - ct), v),
        "bottom": ("z", -1, cz - h, (cx - w + cb, cx + w - cb), v),
        "right": ("x", 1, cx + w, (cz - h + cb, cz + h - ct), v),
        "left": ("x", -1, cx - w, (cz - h + cb, cz + h - ct), v),
    }


def greebles(builder, rng, spec, faces, count, size=(0.15, 0.9), height=(0.04, 0.25)):
    regions = block_faces(spec)
    mats = ["hull", "hull", "dark", "dark", "metal"]
    for face in faces:
        axis, sign, plane, (u0, u1), (v0, v1) = regions[face]
        if u1 - u0 < 0.2 or v1 - v0 < 0.2:
            continue
        for _ in range(count):
            su = min(rng.uniform(*size), (u1 - u0) * 0.8)
            sv = min(rng.uniform(*size) * 1.5, (v1 - v0) * 0.5)
            sh = rng.uniform(*height)
            u = rng.uniform(u0 + su / 2, u1 - su / 2)
            y = rng.uniform(v0 + sv / 2, v1 - sv / 2)
            off = plane + sign * (sh / 2 - 0.02)
            if axis == "z":
                builder.box((u, y, off), (su, sv, sh), rng.choice(mats))
            else:
                builder.box((off, y, u), (sh, sv, su), rng.choice(mats))


def stripes(builder, x, z, y0, count, width=0.16, gap=0.14, length=0.8, lean=0.35):
    """Row of slanted orange hazard stripes on a side face at x."""
    for i in range(count):
        y = y0 + i * (width + gap)
        builder.box((x, y, z), (0.03, width, length), "trim", Euler((lean, 0, 0)))


def build_ship(collection, root):
    rng = random.Random(1337)

    hull = MeshBuilder("Hauler_Hull", ["hull", "dark", "metal", "trim", "window"])
    for spec, mat in (
        (ENGINEERING, "dark"),
        (MID_HULL, "hull"),
        (DORSAL_HUMP, "hull"),
        (PORT_POD, "hull"),
        (STARBOARD_POD, "hull"),
        (EMBLEM_POD, "hull"),
        (BELLY, "dark"),
        (DORSAL_SPINE, "hull"),
        (STARBOARD_BOOM, "dark"),
        (PORT_BOOM, "dark"),
    ):
        y0, y1, cx, cz, w, h, ct, cb, end = spec
        hull.block(y0, y1, cx, cz, w, h, ct, cb, end, mat)
    hull.loft(FORWARD_HULL, "hull")

    # Armour plates layered over the hull sides and dorsal hump.
    hull.block(-5.8, 1.8, 2.15, -1.35, 0.6, 0.75, 0.3, 0.3, 0.3, "hull")
    hull.block(-5.8, 1.8, -2.15, -1.35, 0.6, 0.75, 0.3, 0.3, 0.3, "hull")
    hull.block(-6.2, -1.6, 0.0, 3.05, 1.2, 0.18, 0.15, 0.05, 0.4, "dark")
    hull.block(4.2, 10.5, 1.55, -0.55, 0.5, 0.9, 0.3, 0.3, 0.4, "hull")
    hull.block(4.2, 10.5, -1.55, -0.55, 0.5, 0.9, 0.3, 0.3, 0.4, "hull")

    # Dorsal cargo hatch: raised frame with a lid.
    hull.block(3.0, 8.0, 0.0, 2.6, 1.0, 0.08, 0.05, 0.05, 0.1, "dark")
    hull.block(3.25, 7.75, 0.0, 2.66, 0.85, 0.06, 0.04, 0.04, 0.08, "hull")

    # Nose cap and sensor block.
    hull.block(11.9, 12.7, 0.0, 0.8, 0.7, 0.45, 0.2, 0.2, 0.15, "dark")
    hull.box((0.0, 12.0, 1.62), (0.6, 0.5, 0.2), "dark")

    # Boom tips: forked prongs, plus antenna spikes over the nose.
    for spec in (STARBOARD_BOOM, PORT_BOOM):
        y0, y1, cx, cz, w, h = spec[:6]
        for dx in (-w * 0.45, w * 0.45):
            hull.cylinder((cx + dx, y1 + 1.2, cz + 0.1), 0.07, 2.4, "metal", segments=8)
            hull.cylinder((cx + dx, y1 + 2.65, cz + 0.1), 0.12, 0.5, "metal", radius2=0.0, segments=8)
        hull.box((cx, y1 + 0.15, cz), (w * 1.6, 0.3, h * 0.8), "metal")
    for dx in (-0.35, 0.35):
        hull.cylinder((dx, 13.6, 1.25), 0.05, 3.0, "metal", segments=8)
        hull.cylinder((dx, 15.3, 1.25), 0.09, 0.4, "metal", radius2=0.0, segments=8)

    # Orange markings.
    stripes(hull, 2.66, -0.6, -4.6, 4, length=1.0)
    stripes(hull, -2.66, -0.6, -4.6, 4, length=1.0, lean=-0.35)
    stripes(hull, 2.67, -1.15, 5.0, 5, width=0.12, gap=0.12, length=0.45)
    stripes(hull, -2.27, -1.25, 6.5, 4, width=0.12, gap=0.12, length=0.4, lean=-0.35)
    stripes(hull, 3.71, 0.7, -5.3, 3, length=0.9)
    hull.box((2.65, -3.3, -1.5), (0.03, 1.4, 0.12), "trim")
    hull.box((1.0, 12.3, 1.31), (0.35, 0.25, 0.03), "trim")
    # Hexagonal company emblem on the starboard pod.
    hull.cylinder((3.41, 2.0, -0.6), 0.75, 0.04, "trim", axis="X", segments=6)
    hull.cylinder((3.43, 2.0, -0.6), 0.6, 0.04, "dark", axis="X", segments=6)
    hull.cylinder((3.45, 2.0, -0.6), 0.45, 0.04, "trim", axis="X", segments=6)

    # Engineering section details: pipes, a tank and an observation window.
    for z in (-0.9, -0.5):
        hull.cylinder((1.95, -9.8, z), 0.12, 5.0, "metal", segments=10)
        hull.cylinder((-1.95, -9.8, z), 0.12, 5.0, "metal", segments=10)
    hull.cylinder((1.75, -10.6, 0.9), 0.35, 1.6, "metal", segments=16)
    hull.box((1.92, -8.6, 0.45), (0.05, 0.9, 0.55), "window")
    hull.box((1.95, -8.6, 0.45), (0.04, 1.05, 0.7), "dark")

    hull.build(collection, root)

    detail = MeshBuilder("Hauler_Greebles", ["hull", "dark", "metal"])
    greebles(detail, rng, MID_HULL, ["right", "left", "bottom"], 10)
    greebles(detail, rng, DORSAL_HUMP, ["top"], 10)
    greebles(detail, rng, ENGINEERING, ["top", "right", "left", "bottom"], 9, size=(0.1, 0.5))
    greebles(detail, rng, STARBOARD_POD, ["top", "right"], 6)
    greebles(detail, rng, PORT_POD, ["top", "left"], 6)
    greebles(detail, rng, BELLY, ["bottom"], 12)
    greebles(detail, rng, DORSAL_SPINE, ["right", "left"], 5, size=(0.1, 0.35))
    greebles(detail, rng, STARBOARD_BOOM, ["right", "bottom"], 6, size=(0.08, 0.35), height=(0.03, 0.12))
    greebles(detail, rng, PORT_BOOM, ["left", "bottom"], 5, size=(0.08, 0.3), height=(0.03, 0.12))
    greebles(detail, rng, EMBLEM_POD, ["bottom"], 5)
    detail.build(collection, root, bevel=0.015)

    engine = MeshBuilder("Hauler_Engine", ["hull", "dark", "metal", "trim_glow", "engine_glow"])
    engine.cylinder((0, -12.8, 0), 2.0, 0.9, "dark", segments=32)
    engine.cylinder((0, -14.1, 0), 2.3, 1.8, "hull", segments=32)
    engine.cylinder((0, -14.1, 0), 2.34, 0.18, "trim_glow", segments=32)
    for y in (-13.4, -14.8):
        engine.cylinder((0, y, 0), 2.38, 0.12, "metal", segments=32)
    engine.cylinder((0, -15.15, 0), 2.0, 0.3, "dark", segments=32)
    engine.cylinder((0, NOZZLE_Y - 0.15, 0), 1.75, 0.6, "metal", radius2=1.35, segments=32, caps=False)
    engine.cylinder((0, -15.2, 0), 1.3, 0.05, "engine_glow", segments=32)
    for i in range(12):
        a = i / 12 * math.tau
        engine.box(
            (math.cos(a) * 2.32, -14.5, math.sin(a) * 2.32),
            (0.25, 0.5, 0.25),
            "metal",
            Euler((0, -a, 0)),
        )
    engine.build(collection, root)

    # Built in nozzle-local space (0 at the nozzle, extending to -PLUME_LENGTH) for the fade shader.
    plume = MeshBuilder("Hauler_Plume", ["plume"])
    plume.cylinder((0, -PLUME_LENGTH / 2, 0), 3.2, PLUME_LENGTH, "plume", radius2=1.3, segments=32, caps=False)
    plume_obj = plume.build(collection, root, bevel=0)
    plume_obj.data.shade_smooth()
    plume_obj.location = (0, NOZZLE_Y, 0)
    plume_obj.visible_shadow = False


# ---------------------------------------------------------------------------
# Presentation scene (camera, lights, starfield)
# ---------------------------------------------------------------------------


def setup_scene(scene, collection):
    world = bpy.data.worlds.new("Space")
    scene.world = world
    nt = world.node_tree
    bg = nt.nodes.get("Background") or nt.nodes.new("ShaderNodeBackground")
    out = nt.nodes.get("World Output") or nt.nodes.new("ShaderNodeOutputWorld")
    nt.links.new(bg.outputs["Background"], out.inputs["Surface"])
    coords = nt.nodes.new("ShaderNodeTexCoord")
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 500.0
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.79
    ramp.color_ramp.elements[0].color = (0.0, 0.0, 0.0, 1.0)
    ramp.color_ramp.elements[1].position = 0.82
    ramp.color_ramp.elements[1].color = (0.6, 0.65, 0.8, 1.0)
    nt.links.new(coords.outputs["Generated"], noise.inputs["Vector"])
    nt.links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 1.0

    def light(name, kind, energy, location, direction, color=(1, 1, 1), size=None):
        data = bpy.data.lights.new(name, kind)
        data.energy = energy
        data.color = color
        if size is not None:
            data.size = size
        obj = bpy.data.objects.new(name, data)
        obj.location = location
        obj.rotation_euler = Vector(direction).to_track_quat("-Z", "Y").to_euler()
        collection.objects.link(obj)
        return obj

    light("Key_Sun", "SUN", 3.5, (20, -10, 30), (-0.55, 0.25, -0.8))
    light("Fill", "AREA", 4000.0, (25, 15, -15), (-0.8, -0.4, 0.5), (0.6, 0.7, 1.0), 20)
    light("Rim", "AREA", 3000.0, (-25, 20, 10), (0.7, -0.6, -0.3), (0.9, 0.9, 1.0), 15)
    light("Engine_Light", "POINT", 4000.0, (0, -19, 0), (0, 0, -1), (0.4, 0.6, 1.0))

    cam_data = bpy.data.cameras.new("Camera")
    cam_data.lens = 50
    cam_data.clip_end = 500
    cam = bpy.data.objects.new("Camera", cam_data)
    collection.objects.link(cam)
    cam.location = Vector((52.0, -24.0, 38.0))
    target = Vector((0.0, -2.0, 0.0))
    look = (target - cam.location).to_track_quat("-Z", "Y")
    roll = Quaternion((0, 0, 1), math.radians(-28))
    cam.rotation_euler = (look @ roll).to_euler()
    scene.camera = cam

    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = next((e for e in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT") if e in engines), "CYCLES")
    scene.render.resolution_x = 1400
    scene.render.resolution_y = 700
    scene.render.resolution_percentage = 100
    transforms = [i.identifier for i in scene.view_settings.bl_rna.properties["view_transform"].enum_items]
    scene.view_settings.view_transform = "AgX" if "AgX" in transforms else "Filmic"


def main():
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--save", help="Path of the .blend file to write")
    parser.add_argument("--render", help="Path of a preview PNG to render")
    args = parser.parse_args(argv)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene

    ship_col = bpy.data.collections.new("Hauler")
    scene.collection.children.link(ship_col)
    stage_col = bpy.data.collections.new("Presentation")
    scene.collection.children.link(stage_col)

    root = bpy.data.objects.new("Hauler", None)
    root.empty_display_type = "ARROWS"
    ship_col.objects.link(root)

    create_materials()
    build_ship(ship_col, root)
    setup_scene(scene, stage_col)

    if args.save:
        bpy.ops.wm.save_as_mainfile(filepath=bpy.path.abspath(args.save), compress=True)
    if args.render:
        scene.render.filepath = args.render
        bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    main()
