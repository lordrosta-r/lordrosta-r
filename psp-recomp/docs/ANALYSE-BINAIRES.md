# Analyse des binaires fournis

Analyse statique de trois fichiers issus d'une image UMD PSP.
Aucun déchiffrement n'a été effectué : seuls les en-têtes en clair et les
propriétés statistiques ont été lus.

## 1. Vue d'ensemble

| Fichier | Taille | Magic | Entropie | SHA-1 |
|---|---:|---|---:|---|
| `EBOOT.BIN` | 735 808 (0x0B_3A40) | `~PSP` | **8.000** | `6b297b31ac5044ff969a8bcf9a9a8e4c88948f82` |
| `BOOT.BIN` | 735 468 (0x0B_38EC) | `00 00 00 00` | **0.000** | `42b5b4656eedd8b9610b3fb31e9584d37000f08b` |
| `OPNSSMP.BIN` | 1 048 576 (0x10_0000) | `00 00 00 00` | **0.000** | `3b71f43ff30f4b15b5cd85dd9e95ebc7e84eb5a3` |

Conclusions immédiates :

* `BOOT.BIN` et `OPNSSMP.BIN` sont **intégralement remplis de zéros** (0 octet
  non nul sur la totalité du fichier). Ce n'est pas une corruption du dump :
  c'est le comportement normal d'un UMD commercial. Le module en clair est
  blanchi à la production, seul `EBOOT.BIN` (chiffré) contient le code.
* Corrélation qui confirme le point précédent :
  `taille(BOOT.BIN) == elf_size` déclaré dans l'en-tête de `EBOOT.BIN`
  (735 468 = 0x0B38EC). Les deux fichiers sont bien **le même module**, l'un
  en version chiffrée+signée, l'autre en emplacement réservé vidé.
* `OPNSSMP.BIN` est le module « opening / game sharing » fourni par Sony,
  présent sur la plupart des UMD, ici également blanchi. **Aucune valeur
  pour le reverse** — il ne contient pas de code du jeu.

**→ Seul `EBOOT.BIN` porte de l'information exploitable.**

## 2. En-tête `~PSP` de `EBOOT.BIN`

Décodage de l'en-tête PSP (0x150 octets, en clair) :

| Champ | Offset | Valeur |
|---|---|---|
| magic | 0x00 | `~PSP` (`0x5053507E`) |
| mod_attr | 0x04 | `0x0000` (module utilisateur, pas un module noyau) |
| comp_attr | 0x06 | `0x0000` (**pas de compression gzip** de la charge utile) |
| module version | 0x08 | 1.1 |
| **nom du module** | 0x0A | **`pacmance`** |
| header version | 0x26 | 1 |
| nb segments | 0x27 | 2 |
| elf_size | 0x28 | `0x000B38EC` (735 468) |
| psp_size | 0x2C | `0x000B3A40` (735 808) |
| boot_entry | 0x30 | `0x00000108` |
| module_info offset | 0x34 | `0x0006A28C` |
| bss_size | 0x38 | `0x000A5A2C` (676 KiB de BSS) |
| seg_align | 0x3C | 16, 64 |
| seg_addr | 0x44 | `0x00000000`, `0x00081F80` |
| seg_size | 0x54 | `0x00081F68`, `0x000A6A24` |
| devkit_version | 0x78 | **`0x06020010`** → SDK **6.02** |
| decrypt_mode | 0x7C | 9 |
| comp_size | 0xB0 | `0x000B38EC` (= elf_size, cohérent avec « non compressé ») |
| **TAG KIRK** | 0xD0 | **`0xD9160BF0`** |

### Lecture

* **Layout mémoire** : segment 0 (texte + rodata) de 0 à 0x81F68, segment 1
  (données) à partir de 0x81F80 sur 0xA6A24, plus 0x A5A2C de BSS. Le module
  chargé occupera donc ~**1,5 Mio** en RAM, `.bss` compris — profil d'un jeu
  compact, pas d'un gros titre 3D streamé.
* **`module_info` à 0x6A28C** : c'est là que se trouveront, une fois le module
  déchiffré, `SceModuleInfo` → les tables `.lib.stub` (imports NID) et
  `.lib.ent` (exports). C'est le point d'entrée obligatoire de toute la
  résolution symbolique.
* **`boot_entry = 0x108`** et non 0 : l'entrée n'est pas au début du segment,
  typique d'un crt0 PSPSDK/SDK Sony placé après un petit préambule.
* **Entropie 8.000 sur 100 %** du corps (à partir de 0x150) : chiffrement AES
  intact, aucune zone en clair résiduelle. Aucune chaîne, aucune signature de
  moteur n'est récupérable en l'état.
* **TAG `0xD9160BF0`** : appartient à la famille de tags `0xD916xxF0`
  (EBOOT de jeux, firmwares 5.xx–6.xx), cohérent avec le SDK 6.02 déclaré.
  C'est le tag que `pspdecrypt` utilise pour sélectionner la clé KIRK.

### Identification probable

Le nom de module `pacmance` + SDK 6.02 pointe très probablement vers
**PAC-MAN Championship Edition** (Namco Bandai, PSP). À confirmer avec le
`PARAM.SFO` de l'ISO (champs `TITLE` et `DISC_ID`), qui n'a pas été fourni.

## 3. Ce qui bloque, et l'étape suivante

L'`EBOOT.BIN` est chiffré ; tant qu'il ne l'est pas, **aucun** outil d'analyse
(Ghidra, recompilateur statique, désassembleur) ne peut faire quoi que ce
soit. La chaîne standard côté homebrew est :

```
EBOOT.BIN (~PSP, chiffré)
   └─ pspdecrypt        →  BOOT.BIN / .prx en clair (ELF Allegrex)
        └─ Ghidra + ghidra-allegrex + psp-ghidra-scripts (SonyPSPResolveNIDs.py)
             └─ export symboles/métadonnées
                  └─ PSPRecomp  →  C++ + runtime natif
```

Récupérer un `PARAM.SFO` et un dump complet de l'`ISO` (`USRDIR/`) donnerait
aussi les modules secondaires (`.prx`) et les archives d'assets, souvent plus
parlants que l'EBOOT lui-même pour cartographier le moteur.
