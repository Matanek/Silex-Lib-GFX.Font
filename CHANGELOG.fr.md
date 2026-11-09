# Historique

## [0.2.0] - 2026-11-09

- Distribuer Noto Emoji monochrome et Noto Sans CJK SC avec leurs licences et provenance ; Face.emoji() et Face.cjk() ouvrent les faces embarquées sans recherche système.

- Adopte la déclaration `Result` unique : l’appel ordinaire rend la valeur
  ou arrête le programme sur erreur ; `try appel()` conserve le `Result`. Les
  anciens appels stockant un résultat et propagations deviennent explicites.
  Cette version requiert Silex 0.48.0 ou supérieur et les versions coordonnées
  de ses dépendances.
  `Face.try_open(bytes, index)` cède la place à `Face.decode(bytes, face_index)` ;
  `Face.open(path, ...)` reste dédié aux fichiers.
