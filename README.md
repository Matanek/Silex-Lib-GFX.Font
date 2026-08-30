# GFX.Font

`GFX.Font` possède les fontes portables de GFX. Son premier socle ouvre une
face TTF, OTF ou TTC depuis des octets, vérifie les capacités disponibles et
conserve FreeType et HarfBuzz derrière une ABI C privée identique sur toutes
les cibles.

```text
silex install GFX.Font
```

## Documentation

- [Documentation française](Docs/FR/README.md)
- [English documentation](Docs/EN/README.md)
- [Reproduire les artefacts natifs](Native/README.md)

Le package demande Silex 0.43.0 ou une version plus récente. Il ne découvre
pas les fontes du système et n’expose aucun handle FreeType ou HarfBuzz.
