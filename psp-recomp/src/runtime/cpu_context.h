// Contexte Allegrex passé à chaque fonction recompilée.
//
// Convention retenue par le code émis :
//     void func_08804a10(psp::CpuContext& ctx, psp::Memory& mem);
// Les arguments/retours suivent l'ABI o32 (a0-a3, v0-v1), donc rien à
// traduire côté appelant : le code invité s'appelle lui-même normalement.
#pragma once

#include <cstdint>

namespace psp {

// Noms o32, pour que le code émis reste relisible à la pelle.
enum Gpr : uint32_t {
  kZero = 0, kAt, kV0, kV1, kA0, kA1, kA2, kA3,
  kT0, kT1, kT2, kT3, kT4, kT5, kT6, kT7,
  kS0, kS1, kS2, kS3, kS4, kS5, kS6, kS7,
  kT8, kT9, kK0, kK1, kGp, kSp, kFp, kRa,
};

struct CpuContext {
  uint32_t r[32] = {};
  uint32_t hi = 0;
  uint32_t lo = 0;
  float f[32] = {};       // FPU (coprocesseur 1)
  uint32_t fcr31 = 0;

  // VFPU : 128 registres 32 bits, adressés en scalaire, vecteur 2/3/4 ou
  // matrice selon l'instruction, avec préfixes de swizzle. C'est le morceau
  // le plus coûteux du portage — voir docs/ARCHITECTURE.md.
  float v[128] = {};
  uint32_t vfpu_prefix_s = 0;
  uint32_t vfpu_prefix_t = 0;
  uint32_t vfpu_prefix_d = 0;
  uint32_t vfpu_cc = 0;

  // $zero est câblé : le code émis écrit dedans sans condition, on remet à 0.
  void ClampZero() { r[kZero] = 0; }
};

}  // namespace psp
