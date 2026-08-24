# Architecture — portage PSP par recompilation statique

Cible : transformer un module Allegrex/MIPS PSP en exécutable natif, via
recompilation statique (à la N64Recomp / XenonRecomp / PSPRecomp), plutôt
qu'en émulant la console.

## Principe

On ne cherche pas du C *lisible* (ça, c'est la décompilation Ghidra, pour
comprendre). On cherche du C++ *équivalent et compilable* : une fonction hôte
par fonction invitée, les registres MIPS devenus des champs d'un contexte, la
mémoire invitée devenue un grand bloc plat. Illisible, mais natif et
optimisable par le compilateur hôte.

```
                 ┌───────────────────────────────────────────┐
   EBOOT.BIN ───▶│ 0. Déchiffrement (pspdecrypt)             │
                 └───────────────────┬───────────────────────┘
                                     ▼  ELF Allegrex en clair
                 ┌───────────────────────────────────────────┐
                 │ 1. Analyse (Ghidra + ghidra-allegrex)     │
                 │    • module_info → .lib.stub / .lib.ent   │
                 │    • résolution NID → noms sceXxx         │
                 │    • bornes de fonctions, jump tables     │
                 └───────────────────┬───────────────────────┘
                                     ▼  symbols.toml / *.sym
                 ┌───────────────────────────────────────────┐
                 │ 2. Recompilation (PSPRecomp)              │
                 │    → recompiled/*.cpp (1 TU / fonction)   │
                 └───────────────────┬───────────────────────┘
                                     ▼
   ┌─────────────────────────────────┴─────────────────────────────────┐
   │ 3. Runtime natif                                                   │
   │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │
   │  │ mémoire  │ │ CPU ctx  │ │  VFPU    │ │   HLE    │ │ platform │ │
   │  │ plate    │ │ + reloc  │ │ 128 regs │ │  sceXxx  │ │ SDL/GPU  │ │
   │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘ │
   └────────────────────────────────────────────────────────────────────┘
```

## Arborescence

```
psp-recomp/
├── docs/
│   ├── ARCHITECTURE.md        ← ce document
│   ├── ANALYSE-BINAIRES.md    ← analyse des dumps fournis
│   └── HLE-COVERAGE.md        ← état des sceXxx implémentés
├── tools/
│   ├── psp_header.py          ← parseur d'en-tête ~PSP (fait)
│   ├── extract_modinfo.py     ← module_info + NID (post-déchiffrement)
│   └── nid_db/                ← tables NID → nom de fonction
├── config/
│   └── pacmance.toml          ← profil du jeu : segments, symboles, patches
├── src/
│   ├── runtime/               ← mémoire, contexte CPU, VFPU, dispatch indirect
│   ├── hle/                   ← implémentations natives des modules Sony
│   └── platform/              ← fenêtre, entrées, GE→GPU, audio
├── recompiled/                ← GÉNÉRÉ, non versionné
├── third_party/               ← PSPRecomp, SDL3, etc. (submodules)
└── build/
```

### `src/runtime/` — le socle

* **mémoire plate** : un `uint8_t*` de 32 Mio (64 sur PSP-2000) mappé à
  `0x08000000`. Adresse invitée → hôte par simple masquage, pas de MMU. Il
  faut gérer les miroirs (`0x0`, `0x4`, `0x8` haut) et le scratchpad
  `0x00010000` (16 Kio).
* **contexte CPU** : 32 GPR + HI/LO + FPU. `$zero` câblé, `$sp` initialisé
  par le runtime. Passé en argument à chaque fonction recompilée.
* **VFPU** : le vrai piège de la PSP. 128 registres, vus comme scalaires,
  vecteurs 2/3/4, matrices, avec préfixes de swizzle/saturation. Les jeux
  Sony l'utilisent massivement (matrices, skinning). À traduire vers du SIMD
  hôte quand c'est possible, sinon en scalaire de référence — commencer par
  le scalaire correct, optimiser après.
* **branchement indirect** : `jr $ra` et les jump tables ne se résolvent pas
  statiquement. Table adresse-invitée → pointeur de fonction, plus un
  trampoline de secours.
* **délai de branchement** : chaque branche MIPS exécute l'instruction
  suivante. Le recompilateur doit la placer *avant* le saut dans le C émis ;
  c'est la source n°1 de bugs subtils.

### `src/hle/` — remplacer le firmware

Le code du jeu appelle des NID (`sceGuStart`, `sceIoOpen`, `sceCtrlPeekBufferPositive`…).
Chaque import résolu devient un stub natif. Ordre de priorité :

1. `sceIo*` (fichiers) et `sceKernel*` (threads, sémaphores, allocation) —
   sans ça le jeu ne boote pas.
2. `sceCtrl*` (manette) — trivial, gros gain de testabilité.
3. `sceGu*` / `sceGe*` — le Graphics Engine. On intercepte la display list et
   on la retraduit vers l'API hôte. **C'est le gros du travail.**
4. `sceAudio*`, puis les codecs (`sceMpeg`, `sceAtrac`) — souvent les
   derniers, remplaçables par du silence pendant le bring-up.

Règle : chaque stub non implémenté doit *logger et poursuivre*, pas planter.
On progresse en lisant les logs de NID manquants.

### `config/<jeu>.toml` — le profil

Tout ce qui est spécifique au titre y est isolé, jamais dans `src/` :
adresses de segments, liste de symboles, fonctions à **remplacer à la main**
(les cas que le recompilateur ne sait pas traduire : code auto-modifiant,
boucles d'attente sur un registre matériel, `sceKernelDelayThread` en
busy-wait).

## Plan de bring-up

| Étape | Livrable | Critère de réussite |
|---|---|---|
| 0 | EBOOT déchiffré | ELF valide, sections lisibles |
| 1 | Cartographie | `module_info`, tous les NID importés listés |
| 2 | Recompilation | ça compile, même si ça ne tourne pas |
| 3 | Boot | atteint `main` sans crash, logs de NID manquants |
| 4 | Vidéo | premier frame à l'écran |
| 5 | Jouable | entrées + boucle de jeu stables |
| 6 | Audio / finitions | — |

Ne pas viser l'étape 4 avant que 3 soit propre : un crash graphique sur un
runtime CPU faux est indébogable.

## Points durs, classés

1. **VFPU** — mal documenté, préfixes complexes. Prévoir des tests
   différentiels contre PPSSPP (`ghidra-allegrex` sait s'y attacher en
   debugger via WebSocket, très utile pour comparer les registres).
2. **GE / display list** — il faut réimplémenter un pilote graphique, pas
   juste des stubs.
3. **Threads** — l'ordonnanceur PSP est coopératif et à priorités strictes.
   Le mapper naïvement sur des threads natifs préemptifs casse les jeux qui
   dépendent de l'ordre d'exécution. Un ordonnanceur coopératif maison est
   plus sûr.
4. **Endianness / alignement** : la PSP est little-endian comme x86-64, un
   souci de moins. Mais `lwl`/`lwr` (accès non alignés) doivent être émulés.

## Légal

Le code recompilé dérive du binaire du jeu : il ne se redistribue pas. Ce
dépôt ne contient que des outils et un runtime ; l'utilisateur fournit son
propre dump. C'est le modèle de tous les projets `*Recomp`.
