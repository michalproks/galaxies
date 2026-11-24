import math
import os
import random

import bpy
import bmesh
from mathutils import Vector


OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))


def new_collection(name):
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    return collection


HULL = new_collection("01 | Hull and frame")
ARMOR = new_collection("02 | Armor and payload")
DETAILS = new_collection("03 | Weapons and details")
ENGINE = new_collection("04 | Engine and effects")
STAGE = new_collection("05 | Presentation")


def move_to_collection(obj, collection):
    for existing in tuple(obj.users_collection):
        existing.objects.unlink(obj)
    collection.objects.link(obj)
    return obj


def surface_material(name, color, metallic=0.0, roughness=0.5):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    shader = material.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1.0)
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = roughness
    return material


def glow_material(name, color, strength):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    nodes.clear()
    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = (*color, 1.0)
    emission.inputs["Strength"].default_value = strength
    output = nodes.new("ShaderNodeOutputMaterial")
    material.node_tree.links.new(emission.outputs["Emission"], output.inputs["Surface"])
    return material


HULL_MAT = surface_material("Hull | graphite titanium", (0.105, 0.135, 0.17), 0.76, 0.34)
ARMOR_MAT = surface_material("Armor | ceramic silver", (0.55, 0.59, 0.60), 0.55, 0.3)
PALE_MAT = surface_material("Armor | frost panels", (0.76, 0.78, 0.74), 0.38, 0.29)
DARK_MAT = surface_material("Recesses | carbon", (0.018, 0.027, 0.038), 0.52, 0.32)
GLASS_MAT = surface_material("Glass | smoked blue", (0.025, 0.09, 0.14), 0.72, 0.17)
ORANGE_MAT = surface_material("Markings | rescue orange", (0.92, 0.22, 0.055), 0.25, 0.34)
STEEL_MAT = surface_material("Hardware | brushed steel", (0.34, 0.39, 0.4), 0.82, 0.25)
ENGINE_BLUE = glow_material("Engine | ion blue", (0.008, 0.065, 0.38), 2.2)
ENGINE_CORE = glow_material("Engine | plasma core", (0.12, 0.55, 1.0), 4.0)
STAR_MAT = glow_material("Backdrop | stars", (0.54, 0.69, 0.82), 1.4)


def add_box(name, location, dimensions, material, collection, bevel=0.08, rotation=None):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    if rotation is not None:
        obj.rotation_euler = rotation
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(material)
    if bevel > 0:
        modifier = obj.modifiers.new("Machined edge breaks", "BEVEL")
        modifier.width = bevel
        modifier.segments = 2
        obj.modifiers.new("Corner normals", "WEIGHTED_NORMAL")
    return move_to_collection(obj, collection)


def add_cylinder_x(name, x, radius, depth, material, collection, vertices=40):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices,
        radius=radius,
        depth=depth,
        location=(x, 0.0, 0.0),
        rotation=(0.0, math.pi / 2.0, 0.0),
    )
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    bevel = obj.modifiers.new("Rim bevel", "BEVEL")
    bevel.width = 0.035
    bevel.segments = 2
    obj.modifiers.new("Corner normals", "WEIGHTED_NORMAL")
    return move_to_collection(obj, collection)


def add_beam(name, start, end, radius_start, radius_end, material, collection, vertices=8):
    start = Vector(start)
    end = Vector(end)
    direction = end - start
    bpy.ops.mesh.primitive_cone_add(
        vertices=vertices,
        radius1=radius_start,
        radius2=radius_end,
        depth=direction.length,
        location=(start + end) / 2.0,
    )
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    obj.data.materials.append(material)
    return move_to_collection(obj, collection)


def add_torus_x(name, x, major_radius, minor_radius, material, collection):
    bpy.ops.mesh.primitive_torus_add(
        major_radius=major_radius,
        minor_radius=minor_radius,
        major_segments=48,
        minor_segments=12,
        location=(x, 0.0, 0.0),
        rotation=(0.0, math.pi / 2.0, 0.0),
    )
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return move_to_collection(obj, collection)


def create_hull():
    stations = [
        (-7.35, 1.05, 0.98, -0.78),
        (-6.2, 1.85, 1.35, -1.08),
        (-4.2, 2.38, 1.68, -1.34),
        (-1.6, 2.55, 1.84, -1.42),
        (1.55, 2.05, 1.48, -1.16),
        (4.25, 1.42, 1.08, -0.78),
        (6.2, 0.58, 0.52, -0.34),
        (6.85, 0.08, 0.10, -0.08),
    ]
    ring_size = 6
    vertices = []
    for x, width, top, bottom in stations:
        shoulder = top * 0.2 + bottom * 0.8
        section = [
            (-width * 0.62, top),
            (width * 0.62, top),
            (width, shoulder),
            (width * 0.72, bottom),
            (-width * 0.72, bottom),
            (-width, shoulder),
        ]
        vertices.extend((x, y, z) for y, z in section)

    faces = []
    faces.append(tuple(range(ring_size - 1, -1, -1)))
    faces.append(tuple((len(vertices) - ring_size) + i for i in range(ring_size)))
    for ring in range(len(stations) - 1):
        for side in range(ring_size):
            a = ring * ring_size + side
            b = ring * ring_size + (side + 1) % ring_size
            c = (ring + 1) * ring_size + (side + 1) % ring_size
            d = (ring + 1) * ring_size + side
            faces.append((a, b, c, d))

    mesh = bpy.data.meshes.new("Six-sided armored hull mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.validate()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    obj = bpy.data.objects.new("Primary pressure hull", mesh)
    HULL.objects.link(obj)
    obj.data.materials.append(HULL_MAT)
    bevel = obj.modifiers.new("Hull edge highlights", "BEVEL")
    bevel.width = 0.045
    bevel.segments = 1
    obj.modifiers.new("Weighted hull normals", "WEIGHTED_NORMAL")
    return obj


def create_polygon_prism(name, points, z_bottom, z_top, material, collection, bevel=0.04):
    count = len(points)
    vertices = [(x, y, z_bottom) for x, y in points]
    vertices += [(x, y, z_top) for x, y in points]
    faces = [tuple(range(count - 1, -1, -1)), tuple(range(count, 2 * count))]
    for i in range(count):
        faces.append((i, (i + 1) % count, (i + 1) % count + count, i + count))
    mesh = bpy.data.meshes.new(name + " mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.validate()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.data.materials.append(material)
    if bevel > 0:
        modifier = obj.modifiers.new("Panel edge chamfers", "BEVEL")
        modifier.width = bevel
        modifier.segments = 2
        obj.modifiers.new("Panel normals", "WEIGHTED_NORMAL")
    return obj


def add_text(name, body, location, size, material):
    curve = bpy.data.curves.new(name, type="FONT")
    curve.body = body
    curve.size = size
    curve.extrude = 0.003
    curve.bevel_depth = 0.0015
    curve.align_x = "CENTER"
    obj = bpy.data.objects.new(name, curve)
    DETAILS.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (math.pi / 2.0, 0.0, 0.0)
    obj.data.materials.append(material)
    return obj


def aim_at(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def add_area_light(name, location, target, power, size, color):
    data = bpy.data.lights.new(name, type="AREA")
    data.energy = power
    data.shape = "DISK"
    data.size = size
    data.color = color
    obj = bpy.data.objects.new(name, data)
    STAGE.objects.link(obj)
    obj.location = location
    aim_at(obj, target)
    return obj


def build_ship():
    create_hull()

    # Raised dorsal blocks break up the long pressure hull into armored sections.
    add_box("Aft cargo deck", (-4.45, 0.0, 1.34), (2.65, 2.85, 0.72), ARMOR_MAT, ARMOR, 0.16)
    add_box("Central dorsal spine", (-0.85, 0.0, 1.63), (5.0, 2.34, 0.92), PALE_MAT, ARMOR, 0.2)
    add_box("Bridge housing", (3.18, 0.0, 1.31), (2.72, 1.72, 0.7), ARMOR_MAT, ARMOR, 0.15)
    add_box("Bridge canopy", (3.56, 0.0, 1.73), (1.32, 1.18, 0.18), GLASS_MAT, ARMOR, 0.07)
    add_box("Dorsal sensor housing", (-2.72, 0.0, 2.18), (1.18, 1.55, 0.34), HULL_MAT, ARMOR, 0.08)
    add_box("Upper armor ridge", (-0.6, 0.0, 2.16), (3.1, 0.92, 0.16), ARMOR_MAT, ARMOR, 0.06)

    # Broad side pods and the layered panels that make the hull read as a working ship.
    for side in (-1, 1):
        y = side * 2.12
        add_box("Port cargo sponson" if side < 0 else "Starboard cargo sponson",
                (-0.55, y, -0.16), (5.1, 1.12, 1.48), HULL_MAT, ARMOR, 0.16)
        add_box("Sponson armor plate", (-0.65, side * 2.72, -0.12), (3.55, 0.12, 1.0),
                ARMOR_MAT, ARMOR, 0.08)
        add_box("Sponson inset", (-0.62, side * 2.795, -0.12), (2.95, 0.035, 0.55),
                DARK_MAT, DETAILS, 0.035)
        add_box("Aft equipment pod", (-4.42, side * 1.72, -0.12), (2.55, 0.9, 1.52),
                ARMOR_MAT, ARMOR, 0.13)
        add_box("Forward shoulder plate", (2.0, side * 1.53, 0.28), (2.7, 0.72, 0.82),
                PALE_MAT, ARMOR, 0.14, rotation=(0.0, -0.08, 0.0))

        # Recessed vents and orange caution bars on both outer faces.
        for index in range(5):
            x = -1.8 + index * 0.43
            add_box("Sponson cooling slot", (x, side * 2.81, -0.08), (0.14, 0.045, 0.42),
                    DARK_MAT, DETAILS, 0.025)
        for index in range(3):
            x = 1.45 + index * 0.32
            add_box("Orange identification stripe", (x, side * 2.815, -0.15), (0.18, 0.04, 0.66),
                    ORANGE_MAT, DETAILS, 0.025)

        # Small service hatches and panel seams on the aft shoulder.
        add_box("Aft side hatch", (-4.46, side * 2.19, 0.72), (1.26, 0.08, 0.68),
                DARK_MAT, DETAILS, 0.055)
        for index in range(4):
            add_box("Hatch rib", (-4.9 + index * 0.28, side * 2.245, 0.72), (0.055, 0.045, 0.48),
                    STEEL_MAT, DETAILS, 0.018)

    # Top-side armor panels, heat vents, and repeated fasteners.
    create_polygon_prism(
        "Dorsal forward armor leaf",
        [(1.15, -0.84), (3.1, -0.7), (4.25, -0.3), (4.25, 0.3),
         (3.1, 0.7), (1.15, 0.84), (0.65, 0.48), (0.65, -0.48)],
        1.78, 1.9, PALE_MAT, ARMOR, 0.045,
    )
    for index in range(6):
        add_box("Spine heat vent", (-2.0 + index * 0.4, 0.0, 2.255), (0.19, 0.68, 0.055),
                DARK_MAT, DETAILS, 0.02)
    for side in (-1, 1):
        for x in (-5.35, -4.95, -3.95, -3.55, -1.8, -1.4, 0.65, 1.05):
            bpy.ops.mesh.primitive_uv_sphere_add(
                segments=8, ring_count=4, radius=0.055, location=(x, side * 2.79, 0.35)
            )
            bolt = bpy.context.object
            bolt.name = "Armor fastener"
            bolt.scale = (1.0, 0.3, 1.0)
            bolt.data.materials.append(STEEL_MAT)
            move_to_collection(bolt, DETAILS)

    # The long forward lances and their triangulated support rails.
    for side in (-1, 1):
        add_beam("Outer prow lance", (5.35, side * 1.06, 0.18), (11.75, side * 1.28, 0.28),
                 0.27, 0.012, STEEL_MAT, DETAILS, 10)
        add_beam("Lance root sleeve", (5.1, side * 1.06, 0.18), (5.9, side * 1.09, 0.2),
                 0.36, 0.3, ARMOR_MAT, DETAILS, 10)
        add_beam("Lance support rail", (4.75, side * 0.78, -0.28), (8.25, side * 1.25, 0.22),
                 0.105, 0.055, HULL_MAT, DETAILS, 8)
        add_beam("Inner forward needle", (5.3, side * 0.35, 0.04), (9.25, side * 0.52, 0.1),
                 0.12, 0.009, ARMOR_MAT, DETAILS, 8)
    add_beam("Centerline prow spike", (5.35, 0.0, 0.05), (11.25, 0.0, 0.12),
             0.18, 0.006, ARMOR_MAT, DETAILS, 10)
    add_beam("Prow cross brace", (6.05, -1.18, 0.25), (6.05, 1.18, 0.25),
             0.075, 0.075, HULL_MAT, DETAILS, 8)
    add_beam("Forward sensor mast", (4.75, 0.0, 0.7), (5.2, 0.0, 1.65),
             0.095, 0.045, STEEL_MAT, DETAILS, 8)

    # Layered circular drive assembly and a stylized ion exhaust plume.
    add_cylinder_x("Drive casing", -7.35, 1.02, 0.72, DARK_MAT, ENGINE)
    add_cylinder_x("Outer drive ring", -7.79, 1.02, 0.22, STEEL_MAT, ENGINE)
    add_torus_x("Drive rim", -7.94, 0.82, 0.14, ARMOR_MAT, ENGINE)
    add_cylinder_x("Nozzle throat", -7.94, 0.73, 0.14, HULL_MAT, ENGINE)
    add_cylinder_x("Ion emitter", -8.04, 0.56, 0.055, ENGINE_BLUE, ENGINE)
    add_cylinder_x("Emitter core", -8.09, 0.28, 0.06, ENGINE_CORE, ENGINE)
    add_beam("Outer exhaust plume", (-10.0, 0.0, 0.0), (-8.05, 0.0, 0.0),
             0.02, 1.12, ENGINE_BLUE, ENGINE, 32)
    add_beam("Inner exhaust plume", (-9.45, 0.0, 0.0), (-8.05, 0.0, 0.0),
             0.015, 0.65, ENGINE_CORE, ENGINE, 32)
    for side in (-1, 1):
        add_box("Thruster cooling fin", (-6.35, side * 0.74, -0.35), (1.55, 0.16, 0.5),
                ARMOR_MAT, ENGINE, 0.06)

    # Distinctive unit marking and restrained safety accents.
    add_text("Ship number", "59", (-0.55, -2.865, -0.15), 0.62, ORANGE_MAT)
    add_text("Hull registry", "K-09 / DEEP SPACE", (-0.55, -2.867, -0.55), 0.16, PALE_MAT)
    for x in (-5.8, -5.3, -4.8):
        add_box("Aft warning bar", (x, -2.21, -0.84), (0.22, 0.05, 0.4),
                ORANGE_MAT, DETAILS, 0.025)


def build_stage():
    world = bpy.data.worlds.new("Black space")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.0007, 0.0012, 0.002, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.1
    bpy.context.scene.world = world

    camera_data = bpy.data.cameras.new("Hero camera")
    camera = bpy.data.objects.new("Camera | hero three-quarter", camera_data)
    STAGE.objects.link(camera)
    camera.location = (-23.5, -28.0, 20.0)
    aim_at(camera, (0.0, 0.0, 0.0))
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = 27.5
    camera_data.lens = 52.0
    bpy.context.scene.camera = camera

    add_area_light("Key | cool softbox", (1.0, -10.0, 15.0), (0.0, 0.0, 0.0),
                   2200.0, 10.0, (0.74, 0.84, 1.0))
    add_area_light("Fill | port hull", (-4.0, -10.0, 4.0), (-1.0, 0.0, 0.0),
                   1800.0, 8.0, (0.62, 0.76, 1.0))
    add_area_light("Fill | warm bounce", (7.0, 4.0, 8.0), (1.0, 0.0, 0.0),
                   1500.0, 8.0, (1.0, 0.66, 0.43))
    add_area_light("Rim | aft edge", (-8.0, 5.0, 3.0), (-1.0, 0.0, 0.0),
                   1900.0, 7.0, (0.32, 0.56, 1.0))
    add_area_light("Top strip", (-1.0, 1.0, 13.0), (0.0, 0.0, 0.0),
                   1200.0, 6.0, (1.0, 0.88, 0.67))

    bpy.context.view_layer.update()
    random.seed(59)
    for index in range(54):
        local_position = Vector((random.uniform(-16.0, 16.0),
                                 random.uniform(-10.0, 10.0), -58.0))
        position = camera.matrix_world @ local_position
        radius = random.uniform(0.012, 0.03)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=radius, location=position)
        star = bpy.context.object
        star.name = "Distant star"
        star.data.materials.append(STAR_MAT)
        move_to_collection(star, STAGE)

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1440
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.filepath = "//ship_59_preview.png"
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0

def main():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    build_ship()
    build_stage()
    blend_path = os.path.join(OUTPUT_DIR, "ship_59.blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend_path)
    bpy.ops.render.render(write_still=True)
    print("Saved Blender ship model to: " + blend_path)
    print("Rendered ship preview to: " + bpy.context.scene.render.filepath)


if __name__ == "__main__":
    main()
