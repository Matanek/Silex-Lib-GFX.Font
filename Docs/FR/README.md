# Ouvrir une face avec GFX.Font

`GFX.Font` ouvre les fontes TTF, OTF et les collections TTC depuis des octets
déjà possédés par l’application. Une face conserve sa propre copie des octets,
expose des métadonnées portables et ferme automatiquement ses ressources.

[Read this documentation in English.](../EN/README.md)

## Installer le package

```text
silex install GFX.Font
```

GFX.Font demande Silex 0.43.0 ou une version plus récente.

## Ouvrir des octets de fonte

```sx
use GFX.Font

let bytes = embed_bytes("NotoSans.ttf")
match Font.Face.try_open(bytes) {
    failure(error) => { print(error.detail) }
    success(face) => {
        print(face.family_name())
        print("$(face.glyph_count()) glyphes")
    }
}
```

`Face.try_open` distingue les données vides, les données qui ne décrivent pas
une fonte, un indice absent dans une collection, une allocation impossible et
une capacité native indisponible. `Error.kind` est stable ; `Error.detail`
copie un détail UTF-8 destiné au diagnostic.

Une collection accepte `index:` pour sélectionner sa face. `face_count()`
indique les indices disponibles. `capabilities()` précise si la face fournit
des contours, le shaping, des variations ou des glyphes couleur ; ces drapeaux
n’obligent pas le développeur à connaître FreeType ou HarfBuzz.

`Face` ferme normalement ses ressources à la fin de sa durée de vie. `close()`
permet de les libérer plus tôt et retourne `false` lorsqu’elles étaient déjà
fermées.

## Comprendre la Boundary

Le package distribue le même shim C privé pour macOS ARM64, Linux x64, Windows
x64 et Windows ARM64. Il retient les mêmes octets et le même indice de face
pour FreeType 2.14.3 et HarfBuzz 14.2.1. Aucune fonte système, aucun handle
natif et aucun type des deux bibliothèques ne traverse l’API publique.

Windows ARM64 reste une cible reconnue et expérimentale : son archive et sa
liaison constituent une preuve structurelle, pas une exécution native.
