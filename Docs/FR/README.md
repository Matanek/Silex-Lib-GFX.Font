# Faces, instances et variations avec GFX.Font

`GFX.Font` charge des fontes TTF, OTF/CFF et des collections TTC depuis des
octets ou un fichier. Une `Face` représente toujours un indice précis dans les
données encodées ; une `Instance` ajoute une taille logique, des coordonnées de
variation et les synthèses explicitement demandées.

[Read this documentation in English.](../EN/README.md)

## Installer le package

```text
silex install GFX.Font
```

GFX.Font demande Silex 0.43.0 ou une version plus récente.

## Charger une face

`Face.decode` reçoit des octets possédés par l’application. `Face.open` lit le
fichier en entier et le ferme avant de retourner : la face reste donc utilisable
si le fichier est ensuite déplacé ou supprimé.

```sx
use GFX.Font

let bytes = embed_bytes("InterVariable.ttf")
match Font.Face.decode(bytes) {
    failure(error) => { print(error.detail) }
    success(face) => {
        print(face.identity())
        if family = face.family_name() { print(family) }
        print("$(face.glyph_count()) glyphes")
    }
}
```

Une collection accepte `face_index:`. `face_count()` donne le nombre d’indices
valides. `identity()` combine le SHA-256 des octets et cet indice ; le chemin,
l’adresse native et l’ordre de chargement n’y participent pas.

Les erreurs externes restent typées. `file_read`, `invalid_data`,
`face_index_out_of_range` et `unsupported` permettent notamment de distinguer
une lecture impossible, une ressource corrompue, une face absente et une
capacité de décodage indisponible.

## Inspecter la face

`family_name()`, `subfamily_name()` et `postscript_name()` sont optionnels,
car une fonte peut omettre ces noms. `supports(scalar:)` vérifie la couverture
d’un scalar Unicode. `outline_kind()` distingue l’absence de contour, les
contours TrueType quadratiques et les contours CFF cubiques ;
`capabilities().outlines` convient lorsqu’une simple disponibilité suffit.

`face.metrics()` retourne les métriques en em. `units_per_em` conserve la
valeur encodée ; ascender, descender, line gap, hauteur de ligne, underline et
strikeout sont normalisés. L’axe vertical est positif vers le haut, donc un
descender est normalement négatif.

## Créer une instance logique

`Instance.create` est faillible afin de refuser une taille nulle, négative,
infinie ou NaN et toute coordonnée invalide.

```sx
match Font.Face.decode(embed_bytes("InterVariable.ttf")) {
    failure(error) => { print(error.detail) }
    success(face) => {
        match Font.Instance.create(
            face,
            size:18.0,
            variations:[Font.Variation("wght", 560.0)]
        ) {
            failure(error) => { print(error.detail) }
            success(instance) => {
                let metrics = instance.metrics()
                print("hauteur logique : $(metrics.line_height)")
            }
        }
    }
}
```

La taille désigne des unités logiques par em, indépendantes de la densité
physique. Les métriques d’instance sont déjà mises à cette échelle. Deux tailles
partagent l’identité de leur face, mais possèdent des identités d’instance
différentes.

`axes()` expose pour chaque tag OpenType de quatre octets son nom optionnel, son
minimum, sa valeur par défaut et son maximum. `named_instances()` expose les
coordonnées et les noms disponibles, mais ces noms localisés ne servent jamais
de clé stable. Une instance ordonne ses coordonnées effectives selon les axes de
la face ; axes inconnus, doublons, valeurs hors domaine et NaN sont refusés.

`Synthesis(embolden:true)` et `Synthesis(oblique:true)` rendent toute synthèse
explicite et l’intègrent à l’identité de l’instance. `StyleIntent` décrit un
poids, une largeur et une inclinaison demandés ; il ne déclenche ni recherche de
fonte système, ni substitution automatique, ni synthèse implicite.

## Façonner un run Unicode homogène

`Instance.shape` transforme un seul segment UTF-8 homogène en `GlyphRun`. Le
résultat possède le texte, l’instance, la direction, le script, la langue, les
glyphes positionnés et ses mesures. L’appel est faillible parce que les tags et
plages de features dépendent du texte réellement façonné.

```sx
match instance.shape(
    "office",
    Font.ShapeOptions(
        direction:Font.Direction.left_to_right,
        script:"Latn",
        language:"fr",
        features:[Font.Feature("liga", 1), Font.Feature("kern", 1)]
    )
) {
    failure(error) => { print(error.detail) }
    success(run) => {
        for glyph in run.glyphs() {
            let origin = glyph.origin()
            let cluster = glyph.cluster()
            print("$(glyph.id) à $(origin.x), cluster $(cluster.start):$(cluster.end)")
        }
    }
}
```

Les clusters sont des plages d’octets de la chaîne UTF-8 d’origine, toujours
alignées sur des frontières de scalar. Plusieurs glyphes peuvent partager la
même plage, tandis qu’une ligature peut couvrir plusieurs scalars. Leur ordre
suit la direction du run : il est donc normalement descendant pour un run RTL.
`advance()`, `offset()` et `origin()` restent distincts ; l’origine inclut
l’offset de placement et peut être passée directement au consommateur du
glyphe.

`GlyphRun.place_clusters(starts, origins, advance)` déplace ensuite des
clusters déjà façonnés vers des origines explicites, par exemple les cellules
d’un terminal. `starts` emploie les mêmes offsets UTF-8 ordonnés et commence à
zéro pour un texte non vide. Les glyphes, marks et clusters restent ceux du
run ; seule leur origine et l’avance logique composée changent. Une cardinalité
incohérente ou une frontière invalide retourne `invalid_placement`.

`RunMetrics.advance` donne l’avance logique totale. `logical_bounds` décrit la
ligne, `ink_bounds` seulement l’encre effectivement dessinée, et les métriques
de baseline, ascender, descender et hauteur de ligne viennent de la même
instance. Une espace possède une avance sans encre. Une chaîne vide produit un
run vide avec des métriques de ligne définies. Un scalar absent conserve le
glyphe notdef ; `Glyph.is_notdef()` et `GlyphRun.missing_glyph_count()` le
rendent inspectable sans chercher une fonte système.

Une feature porte un tag OpenType de quatre octets ASCII imprimables, une
valeur et une plage optionnelle `[start, end)` en offsets UTF-8. `end:-1`
signifie la fin du texte. Une plage qui coupe un scalar est refusée. Les
features restent ordonnées et font partie de la clé du cache.

Quand elles sont omises, la direction et le script sont déduits du premier
contenu non ambigu ; un texte vide ou neutre retombe sur LTR et `Zyyy`. La
langue omise est `und`. Passer ces propriétés explicitement est recommandé
pour un texte éditorial connu. Chaque instance conserve au plus 64 résultats
de shaping ; taille, variations, synthèses, texte et options distincts ne
partagent jamais une entrée.

Un run ne contient ni saut de ligne ni paragraphe bidi mixte. Le fallback
multi-face, la césure, la justification et le wrapping restent la
responsabilité d’une future couche de paragraphe. L’appelant segmente ces cas
en runs homogènes et peut composer leurs résultats.

## Extraire les contours vectoriels

`Instance.outline(glyph_id:)` retourne un contour dans l’espace em, avec la
baseline pour origine et l’axe Y positif vers le haut. Les commandes
`move_to`, `line_to`, `quadratic_to`, `cubic_to` et `close` conservent les
courbes TrueType ou CFF d’origine. Les contours composites sont déjà résolus,
chaque variante porte seulement les points qui lui sont utiles, les bornes sont
analytiques, l’avance nominale est accessible, et `fill_rule` vaut
explicitement `non_zero` afin que les contreformes restent des trous.

```sx
match instance.shape("Bé") {
    failure(error) => { print(error.detail) }
    success(run) => {
        match run.outlines() {
            failure(error) => { print(error.detail) }
            success(placed) => {
                for glyph in placed {
                    if outline = glyph.outline {
                        print("$(glyph.glyph_id): $(outline.contours.count()) contours")
                    }
                }
            }
        }
    }
}
```

Le `Result` distingue une erreur d’extraction d’un contour indisponible. Dans
un résultat valide, `null` signifie que le glyphe n’a pas de représentation
vectorielle ; un `GlyphOutline` présent mais vide représente au contraire un
glyphe vectoriel sans encre, comme une espace. `GlyphRun.outlines()` conserve
pour chaque glyphe façonné son identifiant, son origine, son cluster et une
transformation em-vers-unités-logiques.

La décomposition est mise en cache par face, identifiant de glyphe et
coordonnées de variation. La taille logique n’entre pas dans cette clé : deux
instances de 12 et 48 unités partagent le même contour normalisé. Chaque
instance conserve en plus jusqu’à 512 valeurs publiques afin que les glyphes
répétés d’un run ne recopient pas leurs points. Le package ne tesselle pas ces
chemins et n’importe ni Canvas ni API GPU ; ces décisions appartiennent aux
consommateurs GFX.

## Rasteriser le petit texte avec hinting

`GlyphRun.rasterize` convertit les glyphes déjà façonnés en une
`Coverage` grayscale 8 bits. Il ne relit pas la chaîne et ne recalcule ni les
clusters, ni les ligatures, ni le kerning. Ce chemin convient au petit texte
d’interface et aux cellules d’un terminal, pour lesquels le hinting FreeType
reste plus net qu’une tessellation vectorielle réduite. Les contours restent
le meilleur choix pour le zoom, les scènes transformées et les effets
géométriques.

À partir d’un `run` valide, cet extrait produit un masque à densité 2 avec deux
pixels physiques de marge :

```sx
match run.rasterize(Font.RasterOptions(
    density:2.0,
    hinting:Font.Hinting.normal,
    antialiasing:Font.Antialiasing.grayscale,
    padding:2
)) {
    failure(error) => { print(error.detail) }
    success(coverage) => {
        print("$(coverage.width) × $(coverage.height)")
        print("baseline physique : $(coverage.baseline)")
    }
}
```

`alpha()` retourne `height * stride` octets, avec un `stride` compact égal à
la largeur. `packed_alpha()` expose le même stockage contigu sans le convertir
en tableau ; c’est le parcours prévu pour Canvas, les uploads et les autres
consommateurs sensibles aux copies. Les lignes sont rangées du haut vers le
bas. `origin` place le coin
supérieur gauche du masque dans le repère physique du run, dont la baseline est
à Y = 0 et l’axe Y pointe vers le haut ; `baseline` donne donc la ligne de cette
baseline relativement au sommet. Cet indice signé peut se trouver hors du
masque, par exemple pour un glyphe entièrement sous la baseline. `bounds`
décrit seulement l’encre en unités logiques et exclut le padding. `advance`
reste l’avance logique du run :
changer la densité ou le hinting ne modifie pas le layout.

Une espace ou une chaîne vide retourne une couverture valide de taille 0×0.
L’espace conserve néanmoins son avance. `maximum_pixels`, fixé à 16 777 216
par défaut, refuse les dimensions ou multiplications excessives avant
l’allocation. Une densité non positive ou non finie, un padding négatif et une
couverture trop grande produisent respectivement `invalid_density`,
`invalid_padding` et `coverage_too_large`.

Les bitmaps de glyphes sont partagés par face, variations, identifiant, taille
physique et options de hinting dans un cache natif limité à 16 Mio. Les
couvertures de runs appartiennent à l’instance et sont limitées séparément à
8 Mio. Une ligne modifiée peut ainsi recomposer des glyphes déjà chauds sans
les rasteriser de nouveau, tandis que les anciennes lignes sont évincées sans
vider le cache de glyphes. L’antialiasing RGB subpixel et les glyphes couleur
ne font pas partie de cette première couverture alpha.

Un renderer retenu peut éviter de recomposer toute une ligne modifiée. Après le
shaping, `Instance.rasterize_glyph(glyph_id, options)` retourne une
`GlyphCoverage` pour l’identifiant exact du run. `left` et `top` placent ce
bitmap relativement à l’origine et à la baseline du glyphe ; `density`,
`alpha()` et `packed_alpha()` suivent le même contrat que la couverture de run.
Le résultat peut être vide pour un glyphe sans encre. Cette API ne remappe
jamais le texte et ne remplace pas les positions du `GlyphRun` : le renderer
doit dessiner les glyphes dans leur ordre avec leurs origines façonnées.

## Utiliser les fontes distribuées

`Face.default()` charge Noto Sans variable, `Face.monospace()` charge Noto Sans
Mono et `Face.pixel()` charge Departure Mono depuis les assets du package. Ces
fonctions retournent un `Result<Face, Error>` comme les autres parcours de
chargement. Departure Mono est une fonte monospace rétro avec une couverture
latine étendue, notamment les accents français. Pour conserver sa grille de
pixels exacte, employer de préférence des tailles logiques multiples de 11.
Les fichiers et leurs licences SIL Open Font License se trouvent dans
`Assets/Fonts` et `Licenses`.

GFX.Font ne cherche pas les fontes installées sur la machine et ne télécharge
rien. Une application compose ses faces à partir de ses assets et de chemins
explicites.

## Frontière native

L’ABI privée v5 est distribuée pour macOS ARM64, Linux x64, Windows x64 et
Windows ARM64. Une face possède ses octets et les vues FreeType/HarfBuzz
correspondantes. Chaque instance possède son propre état HarfBuzz de variation.
L’extraction applique les mêmes coordonnées à FreeType sous verrou, puis met en
cache une décomposition immuable indépendante de la taille. Le protocole de
shaping copie glyphes, clusters, positions et extents avant de détruire son
buffer HarfBuzz. Elle rasterise aussi les glyph IDs déjà façonnés, partage
leurs bitmaps bornés et compose les runs dans un buffer natif avant d’en copier
l’alpha compact. Aucun handle ni type natif ne traverse l’API publique.

Windows ARM64 reste une cible reconnue et expérimentale : l’archive et la
liaison fournissent une preuve structurelle, pas une exécution native.
