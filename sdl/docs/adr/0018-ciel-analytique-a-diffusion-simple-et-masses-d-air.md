# ADR 0018 : ciel analytique à diffusion simple et masses d'air

Statut : accepté (M3-05)

## Contexte

Le ciel de M1-16 est une cubemap cuite au chargement : son disque de soleil ne suit pas les curseurs du
soleil, et rien ne lie la couleur du ciel, celle de la lumière directe ou l'ambiante à la hauteur du soleil.
M3-05 demande un « ciel atmosphérique analytique simplifié, direction du soleil liée à la lumière directionnelle »
dont la couleur soit cohérente avec la hauteur du soleil.

## Décision

- Un modèle à **diffusion simple** de Rayleigh et de Mie avec des **masses d'air** (atmosphère plane,
  `m(mu) = 1 / (mu + 0,025 exp(-11 mu))`), évalué par pixel dans `atmosphere.frag` et à l'identique en C
  (`engine/atmosphere.c`), avec deux plafonds de masse d'air et un facteur de crépuscule (voir docs/atmosphere.md).
- La même fonction C fournit la couleur de la lumière directe (transmittance relative au zénith), sa visibilité
  et l'ambiante relative, que le lanceur applique à `EngLighting` à chaque image.
- Le test compare le shader au C pixel par pixel (HLSL n'a pas d'`#include` ici : les formules sont dupliquées,
  ce test garde la copie alignée).
- L'éclairage par image (cubemap) reste **cuit une fois**, depuis un dégradé équivalent au ciel du soleil initial.

## Alternatives

- **Préetham / Hosek-Wilkie** : donnent des valeurs plus justes, mais des tables de coefficients à reprendre
  d'une publication (risque de recopie fausse, impossible à vérifier ici sans référence) et une couleur du soleil
  qui reste à modéliser à part. Reportable : l'interface (`eng_atmosphere_sky`) ne changerait pas.
- **Hillaire (LUT de transmittance et de diffusion multiple)** : le plus fidèle, plusieurs passes et textures ;
  disproportionné avant le brouillard et les nuages.
- **Cubemap recalculée quand le soleil bouge** : plusieurs centaines de ms de CPU pour le préfiltrage ; inutilisable
  pour un cycle jour/nuit continu. L'ambiante relative remplace ce recalcul, au prix de reflets qui restent figés.
- **Dégradé réglé à la main** (zénith/horizon par hauteur) : simple, mais rien ne garantit la cohérence entre le
  ciel, le soleil et l'ambiante ; ici une seule formule les relie.

## Conséquences

- Qualité : ordres de couleur et de luminosité plausibles (midi bleu, horizon pâle, coucher rouge, nuit sombre),
  pas de valeurs de référence ; à améliorer par Hosek-Wilkie ou Hillaire si le rendu final l'exige (M8).
- Les reflets des métaux ne suivent pas le soleil (limite documentée).
- Coût GPU : un triangle plein écran, une dizaine d'`exp` et un `pow` par pixel ; non mesuré sur vrai GPU.
