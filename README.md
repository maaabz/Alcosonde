
# ALCOSONDE

Sonde immersible de mesure du titre alcoométrique par voie capacitive, pilotée par un microcontrôleur STM32.

ALCOSONDE mesure le pourcentage d'alcool d'un liquide (de 0 à 40 %vol) en exploitant le contraste de permittivité diélectrique entre l'eau et l'éthanol. L'appareil affiche le résultat par plages et signale, par un retour visuel et sonore, l'instant où le liquide atteint la plage visée. Il est pensé pour suivre l'évolution d'une fermentation maison (bière, vin, cidre, hydromel) jusqu'au degré souhaité.

Projet réalisé dans le cadre du module Projet Prototypage à École nationale supérieure des Mines de Saint-Étienne cursus ISMIN (2025-2026).
<img width="3840" height="5120" alt="WhatsApp Image 2026-06-07 at 17 10 56 - Copie" src="https://github.com/user-attachments/assets/73741c32-c920-4771-b0d4-118283b440b3" />  <img width="1156" height="646" alt="image" src="https://github.com/user-attachments/assets/319f604b-a70f-4986-a120-c37041b9cffa" />  <img width="752" height="1012" alt="image" src="https://github.com/user-attachments/assets/5c2d8980-7843-4a4d-ae12-c3f3ec97bab1" /> <img width="1180" height="938" alt="image" src="https://github.com/user-attachments/assets/2965ab1d-c40a-49d6-9342-7cc8081271d5" />



## Sommaire

- [Principe](#principe)
- [Fonctionnalités](#fonctionnalités)
- [Architecture matérielle](#architecture-matérielle)
- [Brochage](#brochage)
- [Firmware](#firmware)
- [Compilation et programmation](#compilation-et-programmation)
- [Calibration](#calibration)
- [Utilisation](#utilisation)
- [Structure du dépôt](#structure-du-dépôt)
- [Matériel](#matériel)
- [Limites connues](#limites-connues)
- [Améliorations possibles](#améliorations-possibles)
- [Auteurs](#auteurs)
- [Licence](#licence)
- [Avertissement](#avertissement)

## Principe

La capacité d'un condensateur plan vaut `C = ε0 · εr · S / d`. En gardant la géométrie fixe (deux plaques de 20×20 mm espacées de 4 mm), la capacité ne dépend plus que de la permittivité `εr` du liquide placé entre les plaques. Or l'eau (`εr ≈ 80`) et l'éthanol (`εr ≈ 25`) ont des permittivités très différentes : un mélange d'eau et d'éthanol a une permittivité intermédiaire, directement liée à son titre alcoométrique.

La sonde forme donc un condensateur dont la capacité varie avec le taux d'alcool. Un circuit conditionneur (oscillateur à relaxation) convertit cette capacité en une fréquence selon `f = K / C`, que le STM32 mesure par Input Capture puis convertit en titre alcoométrique.

## Fonctionnalités

- Mesure du titre alcoométrique de 0 à 40 %vol.
- Affichage par plages de 5 %vol sur écran couleur.
- Sélection de la plage cible par un bouton de consigne.
- Retour visuel (écran vert) et sonore (buzzer) quand le titre entre dans la plage visée.
- Lecture en continu, adaptée au suivi d'une fermentation dans le temps.

## Architecture matérielle

- **Sonde capacitive** : deux plaques de cuivre 20×20 mm espacées de 4 mm, gravées sur FR4, isolées par un film de silicone fluide transparent, maintenues par un support imprimé en 3D.
- **Conditionneur** : oscillateur à relaxation autour d'un AOP quadruple ADA4622-4, au format shield pour s'enficher sur la carte. Convertit la capacité en fréquence (`f = K / C`).
- **Microcontrôleur** : carte STM32 Nucleo F301K8, timer TIM2 à 32 MHz en Input Capture.
- **Périphériques** : écran TFT ILI9341 (240×320, SPI), bouton de consigne, buzzer piézo actif.
- **Alimentation** : port USB de la carte, qui sert aussi à la programmation et au debug.

## Brochage

| Broche | Fonction MCU | Composant |
|--------|--------------|-----------|
| PA0  | TIM2_CH1    | Sortie du conditionneur (Input Capture) |
| PA2  | USART2_TX   | Liaison série VCP (ST-LINK) |
| PA15 | USART2_RX   | Liaison série VCP (ST-LINK) |
| PA8  | GPIO sortie | Buzzer |
| PB0  | GPIO entrée | Bouton de consigne (tirage interne) |
| PB1  | GPIO sortie | Écran : CS |
| PB3  | SPI3_SCK    | Écran : horloge SCK |
| PB5  | SPI3_MOSI   | Écran : données MOSI |
| PB6  | GPIO sortie | Écran : RST |
| PB7  | GPIO sortie | Écran : DC |
| +5V  | Alimentation | Conditionneur |
| +3V3 | Alimentation | Écran |
| GND  | Masse        | Masse commune |


Voici le schéma de branchement global d’ALCOSONDE :

<img width="1672" height="941" alt="image" src="https://github.com/user-attachments/assets/b5cf44ab-d8b5-44a5-bd23-cddb014fbc5f" />

## Firmware

Développé sous STM32CubeIDE avec les bibliothèques HAL. Le programme s'organise en deux parties :

- une routine d'interruption (`HAL_TIM_IC_CaptureCallback`) qui mesure la période du signal à chaque front montant sur PA0 ;
- une boucle principale qui lit le bouton, calcule le titre à partir de la période, met à jour l'écran et déclenche le buzzer.

Chaîne de traitement : période → fréquence (`f = F_CLK / période`) → capacité (`C = K / f − Cp`) → permittivité (`εr = C / pente`) → titre (loi d'Akerlöf linéarisée) → comparaison à la plage de consigne.

## Compilation et programmation

1. Installer STM32CubeIDE.
2. Importer le projet du dossier `firmware/`.
3. Compiler (Build).
4. Brancher la carte en USB et flasher (Run) via le ST-LINK intégré.
5. Ouvrir un terminal série à 115200 bauds pour suivre les mesures.

## Calibration

La sonde se calibre en deux points, à partir de deux milieux de permittivité connue :

1. À l'air (`εr ≈ 1`), plaques sèches : relever la fréquence.
2. Dans l'eau distillée (`εr ≈ 80,1`), plaques immergées sans bulles : relever la fréquence.

À partir de ces deux fréquences, on calcule les coefficients `CP` (offset) et `PENTE`, à mettre à jour dans les `#define` du `main.c`. La constante `K` reste à sa valeur théorique. Il faut recalibrer après tout changement mécanique de la sonde (collage des électrodes, par exemple).

## Utilisation

1. Brancher la carte en USB.
2. Plonger la sonde dans le liquide, plaques entièrement immergées et sans bulles.
3. Appuyer sur le bouton pour choisir la plage de titre visée (les plages défilent de façon cyclique).
4. Lire le titre sur l'écran. Quand il entre dans la plage choisie, l'écran passe au vert et le buzzer émet un bip.

## Structure du dépôt

```
alcosonde/
├── firmware/             # Projet STM32CubeIDE (main.c, configuration CubeMX)
├── hardware/
│   ├── conditionneur/    # Fichiers KiCad du PCB
│   └── sonde/            # Modèles 3D (Fusion 360) du support et du boîtier
├── simulation/           # Modèle COMSOL
├── docs/                 # Rapport et images
└── README.md
```

(à adapter à ton organisation réelle)

## Matériel

- Carte STM32 Nucleo F301K8
- AOP quadruple ADA4622-4 et composants passifs du conditionneur
- Écran TFT ILI9341 240×320 (SPI)
- Bouton poussoir
- Buzzer piézo actif 5 V
- Plaque FR4 cuivrée pour la sonde
- Silicone fluide transparent
- Filament d'impression 3D pour le support et le boîtier

## Limites connues

- La sonde ne distingue pas le sucre de l'alcool : un liquide sucré est lu comme plus alcoolisé qu'il ne l'est. La mesure est donc indicative en cours de fermentation.
- Sensibilité réduite au delà de 30 %vol (la permittivité varie plus lentement avec le titre).
- La mesure dépend de la température du liquide (pas de compensation thermique) : mesurer sur un liquide à l'équilibre.
- Un bruit résiduel subsiste sur la mesure, d'où le choix d'un affichage par plages plutôt que d'une valeur précise.

## Améliorations possibles

- Compteur de bulles de CO2 sur le barboteur, pour lever l'ambiguïté entre sucre et alcool en fermentation.
- Connectivité Bluetooth et historique des mesures sur smartphone.
- Boîtier étanche.
- Mesure en plusieurs points le long d’une sonde plus longue, pour vérifier l’homogénéité d’une cuve de fermentation.
- Filtrage logiciel du signal pour réduire le bruit.

## Licence

licence MIT

## Avertissement

ALCOSONDE est un prototype pédagogique. Il fournit une mesure indicative et n'est pas un instrument certifié. Il ne doit pas servir à évaluer l'aptitude à conduire ni à prendre une décision liée à la consommation d'alcool.
