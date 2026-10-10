# Historique

## [Unreleased]

- Adopte la déclaration `Result` unique : l’appel ordinaire rend la valeur
  ou arrête le programme sur erreur ; `try appel()` conserve le `Result`. Les
  anciens appels stockant un résultat et propagations deviennent explicites.
  Cette révision requiert la toolchain de développement avec cette sémantique
  de `try` ; sa publication doit coordonner les versions du compilateur et des
  packages.
  `Face.try_open(bytes, index)` cède la place à `Face.decode(bytes, face_index)` ;
  `Face.open(path, ...)` reste dédié aux fichiers.
