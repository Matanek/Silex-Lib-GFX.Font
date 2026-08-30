# GFX.Font

`GFX.Font` possède les fontes portables de GFX. Il charge des faces TTF,
OTF/CFF et TTC depuis des octets ou un fichier, expose leurs noms, leur
couverture Unicode, leurs métriques et leurs variations, puis crée des
instances immuables à une taille logique. Une instance façonne ensuite un run
Unicode homogène en glyphes positionnés, clusters UTF-8 et mesures logiques.

```text
silex install GFX.Font
```

## Documentation

- [Documentation française](Docs/FR/README.md)
- [English documentation](Docs/EN/README.md)
- [Reproduire les artefacts natifs](Native/README.md)

Le package demande Silex 0.43.0 ou une version plus récente. Il distribue Noto
Sans variable et Noto Sans Mono, ne découvre pas les fontes système et n’expose
aucun handle FreeType ou HarfBuzz.
