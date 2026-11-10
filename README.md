# Crisp Game Lib — portage Gamebuino AKA

266 mini-jeux à un bouton, dans l'esprit des jeux d'arcade classiques, sur la console **Gamebuino AKA** (ESP32-S3, écran 320×240).

## Jeux et bibliothèque d'origine

- **Bibliothèque et jeux :** Crisp Game Lib, par **ABA Games (Kenta Cho)** — <https://github.com/abagames/crisp-game-lib-portable>
- **Portage SDL et jeux supplémentaires :** **joyrider3774** (Willems Davy) — <https://github.com/joyrider3774/crisp-game-lib-portable-sdl>
- **Licence :** MIT

Tout le mérite des jeux revient à leurs auteurs. Ce dépôt n'est qu'un portage.

## Ce portage

Portage réalisé par Jicehel avec l'outil AKA Port Studio : firmware ESP-IDF 5.5.1 (`idf.py set-target esp32s3 && idf.py build`). Les fichiers du dossier `sdcard_files/` se copient à la racine de la carte SD de la console.

## Licence

Ce portage est distribué sous la même licence que le jeu d'origine : **MIT**, © 2022 ABA Games. Le texte complet est dans [LICENSE](LICENSE) ; le texte du portage SDL est conservé dans [LICENSE_sdl_port.txt](LICENSE_sdl_port.txt).
