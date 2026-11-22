#!/usr/bin/env bash
# Prépare les personnages et armes KayKit (Kay Lousberg, CC0) utilisés par le jeu (M4).
#
#   tools/scripts/kaykit_prepare.sh [dossier_de_téléchargement]
#
# Télécharge les packs « Character Pack : Adventurers 1.0 » et « Character Pack : Skeletons 1.0 »
# (dépôts GitHub officiels de KayKit), puis écrit dans assets/ des fichiers RÉDUITS :
#   assets/characters/knight.glb        chevalier (joueur) : maillages, casque et cape (rigides sous
#                                       les os), squelette, et les clips du jeu (renommés)
#   assets/characters/skeleton_*.glb    ennemis : maillages et squelette (même rig), sans clip
#   assets/props/*.gltf/.bin/.png       armes et boucliers, chacun dans son propre repère
# Les sources téléchargées ne sont pas versionnées ; les fichiers réduits le sont (voir
# assets/LICENCES.md). Relancer ce script après avoir changé la liste des clips (CLIPS ci-dessous).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

dl="${1:-${TMPDIR:-/tmp}/kaykit-sources}"
mkdir -p "$dl/adv" "$dl/skel" assets/characters assets/props
adv=https://raw.githubusercontent.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0/main/addons/kaykit_character_pack_adventures
skel=https://raw.githubusercontent.com/KayKit-Game-Assets/KayKit-Character-Pack-Skeletons-1.0/main/addons/kaykit_character_pack_skeletons

fetch() { [[ -s "$2" ]] || curl -sSfL -o "$2" "$1"; }
fetch "$adv/Characters/gltf/Knight.glb" "$dl/adv/Knight.glb"
fetch "$adv/LICENSE.txt" "$dl/adv/LICENSE.txt"
for c in Minion Warrior Rogue Mage; do
  fetch "$skel/Characters/gltf/Skeleton_$c.glb" "$dl/skel/Skeleton_$c.glb"
done
fetch "$skel/LICENSE.txt" "$dl/skel/LICENSE.txt"
for w in sword_1handed sword_2handed axe_2handed shield_round; do
  fetch "$adv/Assets/gltf/$w.gltf" "assets/props/$w.gltf"
  fetch "$adv/Assets/gltf/$w.bin" "assets/props/$w.bin"
done
fetch "$adv/Assets/gltf/knight_texture.png" "assets/props/knight_texture.png"
fetch "$adv/Assets/gltf/barbarian_texture.png" "assets/props/barbarian_texture.png"
for w in Skeleton_Blade Skeleton_Axe Skeleton_Shield_Small_A Skeleton_Crossbow Skeleton_Staff; do
  fetch "$skel/Assets/gltf/$w.gltf" "assets/props/$w.gltf"
  fetch "$skel/Assets/gltf/$w.bin" "assets/props/$w.bin"
done
fetch "$skel/Assets/gltf/skeleton_texture.png" "assets/props/skeleton_texture.png"

# Clips du jeu : NOM_KAYKIT=nom_moteur (l'ordre n'a pas d'importance : le cuiseur cherche par nom).
CLIPS=(
  Idle=idle Walking_A=walk Running_A=run Walking_Backwards=walk_back
  Running_Strafe_Left=strafe_left Running_Strafe_Right=strafe_right
  Jump_Start=jump_start Jump_Idle=jump_air Jump_Land=jump_land
  Dodge_Backward=dodge_back Dodge_Forward=dodge_forward Dodge_Left=dodge_left Dodge_Right=dodge_right
  1H_Melee_Attack_Slice_Horizontal=slash_h 1H_Melee_Attack_Slice_Diagonal=slash_d
  1H_Melee_Attack_Chop=chop 1H_Melee_Attack_Stab=stab
  2H_Melee_Idle=idle_2h 2H_Melee_Attack_Slice=slash_2h 2H_Melee_Attack_Chop=chop_2h
  2H_Melee_Attack_Spin=spin_2h 2H_Melee_Attack_Stab=stab_2h
  Blocking=block Block=block_raise Block_Hit=block_hit Block_Attack=parry
  Hit_A=hit_a Hit_B=hit_b Death_A=death Death_B=death_b Cheer=cheer
  1H_Ranged_Shoot=shoot Spellcast_Shoot=spell
)
anim_args=()
for c in "${CLIPS[@]}"; do anim_args+=(--anim "$c"); done

# Le chevalier : armes et boucliers retirés (ils viennent des props, attachés par socket).
python3 -I tools/scripts/gltf_extract.py "$dl/adv/Knight.glb" assets/characters/knight.glb \
  --drop-node 1H_Sword --drop-node 2H_Sword --drop-node 1H_Sword_Offhand \
  --drop-node Badge_Shield --drop-node Rectangle_Shield --drop-node Round_Shield \
  --drop-node Spike_Shield "${anim_args[@]}"
for c in Minion Warrior Rogue Mage; do
  lower=$(echo "$c" | tr '[:upper:]' '[:lower:]')
  python3 -I tools/scripts/gltf_extract.py "$dl/skel/Skeleton_$c.glb" \
    "assets/characters/skeleton_$lower.glb"
done
echo "kaykit_prepare : OK"
