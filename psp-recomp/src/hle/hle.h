// HLE : chaque import NID du module devient une fonction native.
//
// Le module_info du binaire déchiffré donne .lib.stub, soit la liste des
// (nom de bibliothèque, NID) réellement importés. On n'implémente que ceux-là,
// dans l'ordre où ils bloquent le boot.
#pragma once

#include <cstdint>

namespace psp {

struct CpuContext;
class Memory;

using HleFn = void (*)(CpuContext&, Memory&);

void RegisterHle(const char* library, uint32_t nid, const char* name, HleFn fn);

// Un NID importé mais non implémenté ne doit PAS faire tomber le process :
// il logge une fois, pose v0 = 0, et rend la main. Le bring-up consiste à
// lire ces logs et à combler dans l'ordre d'apparition.
void HleStubMissing(const char* library, uint32_t nid, CpuContext& ctx);

void RegisterCoreModules();     // sceKernel*, sceIo*  — indispensable au boot
void RegisterCtrlModule();      // sceCtrl*            — entrées
void RegisterGraphicsModules(); // sceGu*, sceGe*      — le gros morceau
void RegisterAudioModules();    // sceAudio*, sceAtrac*

}  // namespace psp
