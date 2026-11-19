"""Export de référence pour docs/blender-export.md (M2-11).

Usage :  blender -b -P tools/blender/make_reference_export.py -- <dossier_de_sortie>

Crée une caisse de 2 m posée au sol (matériau PBR avec la texture damier du dépôt) et une sphère, les
exporte en « glTF séparé » avec les réglages du document (+Y Up, modificateurs appliqués, UV, normales,
matériaux, sans compression ni animation) vers <dossier>/reference.gltf.

ATTENTION : ce script n'a PAS été exécuté par son auteur (Blender n'était pas utilisable dans son
environnement). Les noms de paramètres suivent l'opérateur export_scene.gltf de Blender 4.x ; en cas
d'erreur « unexpected keyword », comparer avec `bpy.ops.export_scene.gltf.get_rna_type()`.
"""
import sys
from pathlib import Path

import bpy

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(argv) != 1:
    sys.exit("usage : blender -b -P make_reference_export.py -- <dossier_de_sortie>")
out_dir = Path(argv[0]).resolve()
out_dir.mkdir(parents=True, exist_ok=True)
texture = Path(__file__).resolve().parents[2] / "assets" / "textures" / "checker.png"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 1.0

# Matériau PBR : couleur de base = damier, métal 0, rugosité 0,5
mat = bpy.data.materials.new("Damier")
mat.use_nodes = True
bsdf = mat.node_tree.nodes["Principled BSDF"]
bsdf.inputs["Metallic"].default_value = 0.0
bsdf.inputs["Roughness"].default_value = 0.5
if texture.exists():
    image_node = mat.node_tree.nodes.new("ShaderNodeTexImage")
    image_node.image = bpy.data.images.load(str(texture))
    mat.node_tree.links.new(image_node.outputs["Color"], bsdf.inputs["Base Color"])

# Caisse de 2 m (côté 2), posée au sol : l'origine est en bas
bpy.ops.mesh.primitive_cube_add(size=2.0, location=(0.0, 0.0, 1.0))
crate = bpy.context.active_object
crate.name = "caisse"
crate.data.materials.append(mat)

# Sphère de rayon 0,5 à côté, sur le sol
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.5, location=(3.0, 0.0, 0.5))
ball = bpy.context.active_object
ball.name = "balle"
ball.data.materials.append(mat)
bpy.ops.object.shade_smooth()

bpy.ops.object.select_all(action="SELECT")
bpy.ops.export_scene.gltf(
    filepath=str(out_dir / "reference.gltf"),
    export_format="GLTF_SEPARATE",
    use_selection=True,
    export_yup=True,
    export_apply=True,
    export_texcoords=True,
    export_normals=True,
    export_tangents=False,
    export_materials="EXPORT",
    export_image_format="AUTO",
    export_cameras=False,
    export_lights=False,
    export_animations=False,
    export_draco_mesh_compression_enable=False,
)
print("export terminé :", out_dir / "reference.gltf")
