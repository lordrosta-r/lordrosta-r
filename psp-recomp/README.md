# psp-recomp

Squelette de portage d'un titre PSP par **recompilation statique** (et non
par émulation) : le code Allegrex/MIPS est traduit une fois pour toutes en
C++ compilable, puis exécuté sur un runtime natif.

* `docs/ARCHITECTURE.md` — l'architecture, le pipeline, le plan de bring-up
  et les points durs classés par difficulté.
* `docs/ANALYSE-BINAIRES.md` — analyse des trois dumps UMD fournis.
* `tools/psp_header.py` — parseur d'en-tête `~PSP` autonome, sans dépendance.
* `config/pacmance.toml` — profil du titre analysé.
* `src/` — squelettes du runtime (mémoire, contexte CPU, dispatch) et du HLE.

## Utilisation

```sh
python3 tools/psp_header.py chemin/vers/EBOOT.BIN
```

Le script trie un dump : fichier blanchi, ELF déjà en clair, ou module `~PSP`
chiffré — et dans ce dernier cas affiche le layout mémoire, le tag KIRK et le
SDK utilisé.

## Chaîne d'outils externe

| Étape | Outil |
|---|---|
| Déchiffrement | [`pspdecrypt`](https://github.com/John-K/pspdecrypt) |
| Désassemblage / analyse | [Ghidra](https://ghidra-sre.org) + [`ghidra-allegrex`](https://github.com/kotcrab/ghidra-allegrex) |
| Résolution des NID | [`psp-ghidra-scripts`](https://github.com/pspdev/psp-ghidra-scripts) |
| Recompilation statique | [`PSPRecomp`](https://github.com/jessicanataliagta/PSPRecomp) |
| Référence de comportement | [PPSSPP](https://github.com/hrydgard/ppsspp) (debugger WebSocket) |

## Portée

Ce dépôt ne contient ni binaire de jeu, ni asset, ni clé. Le code recompilé
dérive du binaire d'origine et n'est pas redistribuable : chaque utilisateur
part de son propre dump, comme dans tous les projets `*Recomp`.
