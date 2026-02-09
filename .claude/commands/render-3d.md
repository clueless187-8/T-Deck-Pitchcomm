# Render 3D Drawings

Render 3D models and PCB designs to static images, or convert between 3D file formats.

## Arguments

- `$ARGUMENTS` — The action to perform. Examples:
  - `render Mechanical/enclosure.stl` — Render an STL file to PNG
  - `render --format jpg XIAO_Catcher_HUD/case.step` — Render a STEP file to JPG
  - `convert enclosure.stl --to step` — Convert STL to STEP format
  - `render --all` — Render all 3D files found in the project
  - `gerber Hardware/gerbers/` — Render Gerber files to a board image

## Instructions

You are a 3D rendering and conversion assistant for electronics/mechanical projects. Your job is to find 3D model files, render them to static images, and convert between formats.

### Supported File Types

| Category | Extensions |
|----------|-----------|
| Mesh/CAD | `.stl`, `.step`, `.stp`, `.obj`, `.3mf`, `.iges`, `.igs` |
| PCB/Gerber | `.gbr`, `.gtl`, `.gbl`, `.gts`, `.gbs`, `.gto`, `.gbo`, `.drl`, `.gm1` |
| Slicer | `.gcode` |

### Workflow

1. **Parse the user's request** from `$ARGUMENTS` to determine the action (render, convert, or gerber).

2. **Locate files**: If a specific file path is given, use it. If `--all` is specified, scan the project for all supported 3D file types using Glob patterns like `**/*.stl`, `**/*.step`, `**/*.stp`, `**/*.gbr`, etc.

3. **Check available tools** by running:
   ```
   which openscad freecad blender gerbv stl-thumb meshconv pcb2gcode 2>/dev/null
   ```
   Select the best available tool for the job:

   **For STL rendering:**
   - Preferred: `stl-thumb <input.stl> <output.png> --size 1920x1080`
   - Alt: `openscad -o output.png --camera=0,0,0,55,0,25,200 --imgsize=1920,1080 -D 'import("input.stl");'`
   - Alt: `blender --background --python-expr "import bpy; bpy.ops.import_mesh.stl(filepath='input.stl'); bpy.ops.render.render(write_still=True)"`

   **For STEP rendering:**
   - Preferred: `freecad --console --run-script` with a Python render script
   - Alt: `blender` with STEP import addon

   **For Gerber rendering:**
   - Preferred: `gerbv --export=png --dpi=600 -o output.png *.gbr`
   - Alt: Use KiCad CLI `kicad-cli pcb render`

   **For format conversion:**
   - STL <-> STEP: `freecad --console` with export script
   - STL <-> OBJ/3MF: `meshconv -c obj input.stl` or Blender
   - Any -> STL (for slicing): `freecad` or `blender`

4. **If no rendering tools are installed**, inform the user and offer to install them:
   ```
   sudo apt-get install openscad gerbv stl-thumb
   pip install numpy trimesh
   ```
   Or suggest using Python with `trimesh` + `pyrender` as a fallback:
   ```python
   import trimesh
   mesh = trimesh.load('model.stl')
   scene = mesh.scene()
   png = scene.save_image(resolution=(1920, 1080))
   with open('output.png', 'wb') as f:
       f.write(png)
   ```

5. **Output location**: Save rendered images and converted files to the `G:/` drive by default. Create subdirectories as needed:
   - Rendered images: `G:/Renders/<project_name>/`
   - Converted files: `G:/Converted/<project_name>/`
   - If `G:/` is not accessible (e.g., Linux without mount), save to `./output/renders/` and `./output/converted/` in the project root, and inform the user.

6. **Report results**: After rendering or converting, report:
   - Input file(s) processed
   - Output file(s) created with full paths
   - Image dimensions / file sizes
   - Any warnings or quality notes

### Integration Notes

- **Shapr3D**: If the user mentions Shapr3D, note that Shapr3D exports to STEP/STL/OBJ. Guide them to export from Shapr3D first, then use this skill to render or convert the exported files.
- **Anycubic Slicer**: For files destined for Anycubic Slicer, convert to STL format and note optimal print settings if visible in the model (wall thickness, overhangs, etc.).
- **Project context**: This project (T-Deck PitchComm) includes mechanical enclosures for electronics. Check the `Mechanical/`, `XIAO_Catcher_HUD/Mechanical/`, and `Schematics/` directories for existing drawings.

### Examples

```
# Render a single STL to PNG
/render-3d render Mechanical/enclosure.stl

# Render all 3D files in the project
/render-3d render --all

# Convert STEP to STL for Anycubic Slicer
/render-3d convert enclosure.step --to stl

# Render Gerber files to board image
/render-3d gerber Hardware/gerbers/

# Render with specific output format
/render-3d render --format jpg --output G:/Renders/HUD_case.jpg XIAO_Catcher_HUD/case.stl
```
