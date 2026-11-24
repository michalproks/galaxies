# Scout Ship Model

Hard-surface 3D model of the scout ship from the reference artwork, built
programmatically with Blender's Python API (`bpy`).

## Files
- `build_ship.py` - generator script (the source of truth; regenerates all meshes)
- `ship.blend` - Blender scene (camera + lights included)
- `ship.glb` - glTF binary export for the game engine / renderer
- `preview.png` - rendered preview from Blender Cycles

## Coordinate convention
`+Y` = nose (forward), `+Z` = up, `+X` = starboard. Length ~7 units, max
half-width ~0.62.

## Regenerating
```
blender --background --python assets/ship/build_ship.py
```
Set `SHIP_OUT` to change the output directory. The script rebuilds the
fuselage, nose cone, canopy ridge, twin cannons, top/side fins, engine +
exhaust glow, hull greebles, markings, camera and lights, then renders
`preview.png` and exports `ship.glb`.
