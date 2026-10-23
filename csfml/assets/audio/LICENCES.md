# Sons et musiques : provenance et licence

## En résumé

**Tous les fichiers audio de `csfml/assets/audio/` sont des créations originales de RedStrings.**
Ils sont calculés par les scripts de [`outils/`](outils/) (synthèse sonore en Python pur, sans
échantillon extérieur), puis encodés en OGG Vorbis par l'encodeur intégré à SFML.

- Aucun enregistrement, échantillon, son ou musique provenant d'un jeu vidéo, d'une série ou d'une
  banque de sons n'est utilisé.
- Aucune mélodie existante n'est reproduite : les mélodies et les grilles d'accords des quatre musiques
  et des jingles ont été écrites pour le projet (voir [`outils/compositions.py`](outils/compositions.py)).
- Ces fichiers sont distribués sous la même licence que le code du dépôt : **MIT**
  (voir [`LICENSE`](../../../LICENSE)).

Tout se régénère à l'identique :

```bash
python3 csfml/assets/audio/outils/generer_sons.py            # tout
python3 csfml/assets/audio/outils/generer_sons.py standard   # un seul pack
```

## Références aux jeux vidéo : du style, pas des copies

Les clins d'œil passent par des **gestes sonores et des timbres** courants dans le jeu vidéo, jamais
par la reprise d'un son ou d'un air précis :

| Clin d'œil | Ce qui est repris | Ce qui ne l'est pas |
|---|---|---|
| Menus de JRPG | deux notes brèves qui montent pour valider | aucun son de menu existant |
| Consoles 8 bits | ondes carrées au rapport cyclique 25 %, arpèges éclair (code Konami, variantes rares) | aucune mélodie de jeu |
| Fanfares de victoire de JRPG | rythme en triolets, cuivres, timbales | mélodie écrite pour RedStrings |
| Jeux de combat | « Round 1 » (montée + gros coup + gong), coups de poing saturés | aucune voix, aucun bruitage existant |
| Jeux d'aventure | étincelles qui montent pour le soin | aucun jingle existant |
| Jeux exigeants (défaite) | grave lent et chœur mineur | aucun son existant |
| Dessins animés | « trombone triste » en trois notes glissées (variante rare de l'erreur) | motif générique, aucun enregistrement |
| Yu-Gi-Oh! | carte claquée sur le disque de duel, compteur de LP qui défile, carte piège révélée, mineur harmonique « égyptien », cinq coups pour l'Interdit, cri de Kuriboh | aucun son ni aucune musique de la série ou des jeux Konami |

Le nom « Yu-Gi-Oh! » et les noms de cartes sont des marques de leurs propriétaires ; ils ne sont cités
que pour décrire le thème. Le mode Yu-Gi-Oh! des illustrations a ses propres conditions, décrites dans
[`THIRD_PARTY_NOTICES.md`](../../../THIRD_PARTY_NOTICES.md).

## Ajouter un son venant d'ailleurs

Avant d'ajouter un fichier qui n'est pas généré par `outils/` :

1. vérifier sa licence : **CC0**, CC BY (avec mention de l'auteur) ou une licence compatible MIT ;
   jamais un son extrait d'un jeu commercial, même « pour rire » ;
2. noter ici sa source (lien), son auteur et sa licence, dans un tableau comme celui ci-dessous ;
3. l'encoder en OGG (ou garder du WAV/FLAC) et le citer dans le `sons.txt` du pack.

| Fichier | Source | Auteur | Licence |
|---|---|---|---|
| *(aucun pour l'instant)* | | | |
