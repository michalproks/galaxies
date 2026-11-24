"""Programmatic hard-surface build of the reference scout ship (refined v2).

Coordinate convention: +Y = nose (forward), +Z = up, +X = starboard.
Run headless: blender --background --python build_ship.py
"""
import bpy, bmesh, math, os, random

OUT_DIR = os.environ.get("SHIP_OUT", "C:/Users/michal/.copilot/repos/copilot-worktrees/galaxies/michalproks-vigilant-giggle/assets/ship")
os.makedirs(OUT_DIR, exist_ok=True)

def reset_scene():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for coll in list(bpy.data.collections):
        if coll.users == 0:
            bpy.data.collections.remove(coll)
    for block in (bpy.data.meshes, bpy.data.materials, bpy.data.images, bpy.data.lights, bpy.data.cameras, bpy.data.node_groups):
        for d in list(block):
            if d.users == 0:
                block.remove(d)

reset_scene()
scene = bpy.context.scene
COLL = scene.collection

def new_obj(name, mesh=None):
    me = mesh or bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    COLL.objects.link(ob)
    return ob

def make_mesh(name, verts, faces):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    return me

def loft(name, sections, caps=True):
    bm = bmesh.new()
    rings = []
    for (y, z, prof) in sections:
        ring = [bm.verts.new((p[0], y, z + p[1])) for p in prof]
        rings.append(ring)
    for a, b in zip(rings, rings[1:]):
        n = len(a)
        for i in range(n):
            bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    if caps:
        bm.faces.new(rings[0][::-1])
        bm.faces.new(rings[-1])
    bm.normal_update()
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return me

def set_smooth(ob):
    for p in ob.data.polygons:
        p.use_smooth = True

def add_bevel(ob, width=0.03, segments=2, angle=math.radians(30)):
    m = ob.modifiers.new("Bevel", "BEVEL")
    m.width = width
    m.segments = segments
    m.limit_method = "ANGLE"
    m.angle_limit = angle
    return m

def cube(name, sx, sy, sz):
    me = make_mesh(name,
        [(-sx/2,-sy/2,-sz/2),(sx/2,-sy/2,-sz/2),(sx/2,sy/2,-sz/2),(-sx/2,sy/2,-sz/2),
         (-sx/2,-sy/2,sz/2),(sx/2,-sy/2,sz/2),(sx/2,sy/2,sz/2),(-sx/2,sy/2,sz/2)],
        [(0,1,2,3),(7,6,5,4),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)])
    return new_obj(name, me)

def cone(name, r1, r2, depth, verts=24):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=r1, radius2=r2, depth=depth)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = new_obj(name, me)
    return ob

def sphere(name, r, verts=24):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=verts, v_segments=verts // 2, radius=r)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = new_obj(name, me)
    set_smooth(ob)
    return ob

def cyl(name, r, depth, verts=24):
    ob = cone(name, r, r, depth, verts)
    set_smooth(ob)
    return ob

def place(ob, loc=(0,0,0), rot=(0,0,0)):
    ob.location = loc
    ob.rotation_euler = rot
    return ob

def shade(ob, color, metallic=0.7, rough=0.45):
    mat = bpy.data.materials.new(ob.name + "_mat")
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = rough
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    return mat

def emit_mat(name, color, strength=8.0):
    mat = bpy.data.materials.new(name)
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
    bsdf.inputs["Emission Strength"].default_value = strength
    return mat

def apply_mat(ob, mat):
    ob.data.materials.clear()
    ob.data.materials.append(mat)

def interp(y, ys, vals):
    """piecewise-linear interpolation"""
    if y <= ys[0]:
        return vals[0]
    for i in range(len(ys) - 1):
        if ys[i] <= y <= ys[i + 1]:
            t = (y - ys[i]) / (ys[i + 1] - ys[i])
            return vals[i] + t * (vals[i + 1] - vals[i])
    return vals[-1]

YS = [0.0, 0.8, 1.6, 2.4, 3.2, 4.0, 4.8, 5.6, 6.2, 6.8]
HALFX = [0.34, 0.46, 0.56, 0.62, 0.56, 0.46, 0.36, 0.26, 0.17, 0.14]
TOPZ  = [0.34, 0.46, 0.55, 0.60, 0.56, 0.48, 0.40, 0.32, 0.26, 0.22]

# ================================================================ FUSELAGE
SECTIONS = [
    (0.0,  0.00, [( 0.10,  0.26), (-0.20,  0.34), (-0.34,  0.12), (-0.26, -0.20),
                   ( 0.00, -0.34), ( 0.26, -0.20), ( 0.34,  0.12), ( 0.20,  0.34)]),
    (0.8,  0.00, [( 0.16,  0.42), (-0.26,  0.46), (-0.46,  0.20), (-0.38, -0.24),
                   ( 0.00, -0.44), ( 0.38, -0.24), ( 0.46,  0.20), ( 0.26,  0.42)]),
    (1.6,  0.02, [( 0.20,  0.50), (-0.34,  0.55), (-0.56,  0.26), (-0.46, -0.26),
                   ( 0.00, -0.50), ( 0.46, -0.26), ( 0.56,  0.26), ( 0.34,  0.50)]),
    (2.4,  0.05, [( 0.22,  0.55), (-0.38,  0.60), (-0.62,  0.30), (-0.50, -0.28),
                   ( 0.00, -0.54), ( 0.50, -0.28), ( 0.62,  0.30), ( 0.38,  0.55)]),
    (3.2,  0.08, [( 0.20,  0.52), (-0.34,  0.56), (-0.56,  0.28), (-0.46, -0.26),
                   ( 0.00, -0.50), ( 0.46, -0.26), ( 0.56,  0.28), ( 0.34,  0.52)]),
    (4.0,  0.10, [( 0.16,  0.46), (-0.28,  0.48), (-0.46,  0.24), (-0.38, -0.22),
                   ( 0.00, -0.42), ( 0.38, -0.22), ( 0.46,  0.24), ( 0.28,  0.46)]),
    (4.8,  0.12, [( 0.12,  0.38), (-0.22,  0.40), (-0.36,  0.20), (-0.30, -0.18),
                   ( 0.00, -0.34), ( 0.30, -0.18), ( 0.36,  0.20), ( 0.22,  0.38)]),
    (5.6,  0.14, [( 0.08,  0.30), (-0.16,  0.32), (-0.26,  0.16), (-0.22, -0.14),
                   ( 0.00, -0.26), ( 0.22, -0.14), ( 0.26,  0.16), ( 0.16,  0.32)]),
    (6.2,  0.16, [( 0.05,  0.24), (-0.10,  0.26), (-0.17,  0.12), (-0.15, -0.10),
                   ( 0.00, -0.20), ( 0.15, -0.10), ( 0.17,  0.12), ( 0.10,  0.26)]),
    (6.8,  0.18, [( 0.02,  0.20), (-0.08,  0.22), (-0.14,  0.10), (-0.12, -0.10),
                   ( 0.00, -0.16), ( 0.12, -0.10), ( 0.14,  0.10), ( 0.08,  0.22)]),
]
fus = new_obj("Fuselage", loft("FuselageMesh", SECTIONS))
add_bevel(fus, 0.025, 2)
shade(fus, (0.42, 0.44, 0.47), 0.75, 0.42)

# ================================================================ NOSE CONE
# Pointed slim sensor wedge at the very front.
nose = cone("Nose", 0.22, 0.05, 0.8, 8)
set_smooth(nose)
place(nose, (0, -0.4, 0.0), (math.radians(-90), 0, 0))
add_bevel(nose, 0.02, 1)
shade(nose, (0.36, 0.38, 0.42), 0.7, 0.45)

# ================================================================ CANOPY RIDGE
canopy = new_obj("Canopy", loft("CanopyMesh", [
    (1.0, 0.46, [(-0.30, 0.00), (-0.44, 0.00), (-0.44, 0.24), (0.44, 0.24), (0.44, 0.00), (0.30, 0.00), (0.30, 0.00), (0.0, 0.00)]),
    (2.0, 0.46, [(-0.34, 0.06), (-0.50, 0.06), (-0.50, 0.40), (0.50, 0.40), (0.50, 0.06), (0.34, 0.06), (0.34, 0.06), (0.0, 0.06)]),
    (3.4, 0.48, [(-0.32, 0.08), (-0.46, 0.08), (-0.46, 0.40), (0.46, 0.40), (0.46, 0.08), (0.32, 0.08), (0.32, 0.08), (0.0, 0.08)]),
    (4.6, 0.48, [(-0.26, 0.02), (-0.38, 0.02), (-0.38, 0.24), (0.38, 0.24), (0.38, 0.02), (0.26, 0.02), (0.26, 0.02), (0.0, 0.02)]),
], caps=True))
add_bevel(canopy, 0.02, 2)
shade(canopy, (0.36, 0.38, 0.41), 0.7, 0.45)

# ================================================================ CANNONS
# Twin barrels mounted on top forward, pointing +Y, tilted slightly up.
for sx in (-0.28, 0.28):
    z = interp(1.0, YS, TOPZ) + 0.30
    bar = cyl("Cannon", 0.045, 2.4, 12)
    place(bar, (sx, 1.4, z), (math.radians(95), 0, 0))
    add_bevel(bar, 0.01, 1)
    shade(bar, (0.18, 0.19, 0.21), 0.85, 0.35)
    # mounting block
    base = cube("GunBase", 0.12, 0.4, 0.12)
    place(base, (sx, 1.4, interp(1.4, YS, TOPZ) + 0.05))
    add_bevel(base, 0.01, 1)
    shade(base, (0.30, 0.31, 0.34), 0.7, 0.5)

# ================================================================ TOP FIN
# Swept vertical fin blade on the rear top, with a flat side (like reference).
fin = new_obj("Fin", loft("FinMesh", [
    (3.4, 0.62, [(-0.03, 0.0), (0.03, 0.0), (0.05, 0.7), (-0.05, 0.7)]),
    (4.4, 0.62, [(-0.03, 0.0), (0.03, 0.0), (0.06, 1.0), (-0.06, 1.0)]),
    (5.4, 0.62, [(-0.03, 0.0), (0.03, 0.0), (0.06, 0.8), (-0.06, 0.8)]),
    (6.4, 0.62, [(-0.03, 0.0), (0.03, 0.0), (0.05, 0.4), (-0.05, 0.4)]),
], caps=True))
add_bevel(fin, 0.02, 2)
shade(fin, (0.34, 0.36, 0.39), 0.7, 0.45)

# ================================================================ SIDE FINS
def side_fin(name, sign):
    pts = [(3.6, sign*0.45, 0.0), (5.0, sign*0.5, 0.05),
           (6.6, sign*1.05, 0.0), (6.6, sign*0.7, -0.22),
           (5.2, sign*0.4, -0.16)]
    bm = bmesh.new()
    n = len(pts)
    bot = [bm.verts.new(p) for p in pts]
    top = [bm.verts.new((p[0], p[1], p[2] + 0.14)) for p in pts]
    bm.faces.new(bot[::-1])
    bm.faces.new(top)
    for i in range(n):
        bm.faces.new((bot[i], bot[(i + 1) % n], top[(i + 1) % n], top[i]))
    bm.normal_update()
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = new_obj(name, me)
    add_bevel(ob, 0.02, 2)
    shade(ob, (0.34, 0.36, 0.39), 0.7, 0.45)
    return ob

side_fin("FinPort", -1)
side_fin("FinStar", 1)

# ================================================================ ENGINE + GLOW
# Main nozzle recessed in the rear face.
nozzle = cone("Engine", 0.24, 0.18, 0.3, 24)
set_smooth(nozzle)
place(nozzle, (0, 6.72, 0.18), (math.radians(90), 0, 0))
shade(nozzle, (0.14, 0.14, 0.17), 0.85, 0.3)

# Exhaust plume: teardrop widest at the nozzle, tapering aft.
plume = sphere("EngineGlow", 0.18)
plume.scale = (0.75, 2.6, 0.75)
place(plume, (0, 7.55, 0.18))
apply_mat(plume, emit_mat("EngineGlow_mat", (0.15, 0.45, 1.0), 3.5))

# Aft thruster pods
for sx in (-0.36, 0.36):
    pod = cyl("Thruster", 0.07, 0.16, 16)
    place(pod, (sx, 6.78, 0.18), (math.radians(90), 0, 0))
    apply_mat(pod, emit_mat("Thruster_mat", (0.30, 0.55, 1.0), 5.0))

# ================================================================ GREEBLES
# Embedded panels hugging the hull surface.
random.seed(11)
for i in range(18):
    sign = random.choice([-1, 1])
    y = random.uniform(0.9, 5.4)
    z = random.uniform(-0.25, 0.32)
    w = random.uniform(0.05, 0.10)
    ln = random.uniform(0.14, 0.34)
    th = random.uniform(0.04, 0.07)
    gx = sign * (interp(y, YS, HALFX) - 0.07)
    g = cube("Greeble", w, ln, th)
    place(g, (gx, y, z))
    add_bevel(g, 0.008, 1)
    shade(g, (0.28, 0.29, 0.32), 0.7, 0.5)

# Top-surface greebles
for i in range(6):
    y = random.uniform(1.4, 4.2)
    g = cube("TopGreeble", random.uniform(0.1, 0.24), random.uniform(0.14, 0.34), 0.04)
    place(g, (random.uniform(-0.18, 0.18), y, interp(y, YS, TOPZ) - 0.02))
    add_bevel(g, 0.008, 1)
    shade(g, (0.28, 0.29, 0.32), 0.7, 0.5)

# ================================================================ MARKINGS
for i in range(4):
    stripe = cube("Stripe", 0.02, 0.35, 0.05)
    place(stripe, (interp(4.5, YS, HALFX) - 0.01, 4.4 + i * 0.28, -0.05))
    apply_mat(stripe, emit_mat("Orange_mat", (1.0, 0.25, 0.05), 2.0))

# ================================================================ SCENE
scene.render.engine = "CYCLES"
scene.cycles.samples = 64
scene.render.resolution_x = 1000
scene.render.resolution_y = 562
scene.view_settings.view_transform = "AgX"
scene.view_settings.look = "AgX - Punchy"
scene.view_settings.exposure = 0.7

world = bpy.data.worlds.get("World") or bpy.data.worlds.new("World")
scene.world = world
bg = world.node_tree.nodes.get("Background")
bg.inputs[0].default_value = (0.012, 0.014, 0.02, 1)
bg.inputs[1].default_value = 1.0

cam = bpy.data.cameras.new("Cam")
cam_obj = bpy.data.objects.new("Camera", cam)
COLL.objects.link(cam_obj)
cam_obj.location = (-5.2, -5.0, 2.6)
cam_obj.data.lens = 45
scene.camera = cam_obj

def add_light(name, loc, energy, size, rot):
    ld = bpy.data.lights.new(name, "AREA")
    ld.energy = energy
    ld.size = size
    lo = bpy.data.objects.new(name, ld)
    COLL.objects.link(lo)
    lo.location = loc
    lo.rotation_euler = rot
    return lo

add_light("Key", (-7, -4, 7), 2600, 6, (math.radians(42), math.radians(22), math.radians(-32)))
add_light("Fill", (-4, 4, 3), 700, 6, (math.radians(55), 0, math.radians(-120)))
add_light("Rim", (4, 6, 4), 1200, 5, (math.radians(-50), 0, math.radians(140)))

target = bpy.data.objects.new("Target", None)
COLL.objects.link(target)
target.location = (0, 3.4, 0.2)
con = cam_obj.constraints.new("TRACK_TO")
con.target = target
con.track_axis = "TRACK_NEGATIVE_Z"
con.up_axis = "UP_Y"

print("Objects:", len(COLL.objects))

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "ship.blend"))
scene.render.filepath = os.path.join(OUT_DIR, "preview.png")
bpy.ops.render.render(write_still=True)

bpy.ops.export_scene.gltf(filepath=os.path.join(OUT_DIR, "ship.glb"), export_format="GLB",
                          use_selection=False, export_apply=True)
print("DONE", OUT_DIR)
