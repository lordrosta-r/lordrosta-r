// Branchements indirects.
//
// `jr $ra`, `jalr`, et les jump tables ne se résolvent pas statiquement :
// le recompilateur émet un appel à Dispatch() avec l'adresse invitée
// calculée à l'exécution. La table est peuplée au démarrage à partir des
// symboles connus.
#pragma once

#include <cstdint>

namespace psp {

struct CpuContext;
class Memory;

using RecompiledFn = void (*)(CpuContext&, Memory&);

void RegisterFunction(uint32_t guest_addr, RecompiledFn fn);

// Renvoie nullptr si l'adresse est inconnue : cas fréquent en début de
// portage (fonction non identifiée par l'analyse, ou pointeur corrompu par
// un bug de traduction en amont). L'appelant doit logger l'adresse plutôt
// que de sauter dans le vide — c'est ce log qui fait avancer la couverture.
RecompiledFn Lookup(uint32_t guest_addr);

void Dispatch(uint32_t guest_addr, CpuContext& ctx, Memory& mem);

}  // namespace psp
