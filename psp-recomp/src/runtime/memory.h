// Mémoire invitée PSP : un bloc plat, pas de MMU.
//
// La PSP mappe la RAM principale à 0x08000000 (32 Mio sur PSP-1000, 64 sur
// 2000+), avec des miroirs non-cachés à 0x4xxxxxxx / 0xAxxxxxxx. Le
// scratchpad (16 Kio) vit à 0x00010000. Toute la traduction d'adresse se
// réduit donc à un masquage — c'est ce qui rend la recompilation statique
// viable ici.
#pragma once

#include <cstdint>
#include <cstddef>

namespace psp {

inline constexpr uint32_t kRamBase = 0x08000000u;
inline constexpr uint32_t kRamSizeFat = 32u << 20;
inline constexpr uint32_t kRamSizeSlim = 64u << 20;
inline constexpr uint32_t kScratchBase = 0x00010000u;
inline constexpr uint32_t kScratchSize = 16u << 10;

// Le masque efface le bit de miroir non-caché et le segment haut : les trois
// vues d'une même page retombent sur le même octet hôte.
inline constexpr uint32_t kAddrMask = 0x1FFFFFFFu;

class Memory {
 public:
  explicit Memory(uint32_t ram_size = kRamSizeSlim);
  ~Memory();

  Memory(const Memory&) = delete;
  Memory& operator=(const Memory&) = delete;

  // Adresse invitée -> pointeur hôte. Pas de vérification de bornes en
  // release : le coût par accès serait rédhibitoire. Activer PSP_MEM_CHECKED
  // pendant le bring-up, c'est là que se trouvent les bugs de traduction.
  uint8_t* Host(uint32_t addr);
  const uint8_t* Host(uint32_t addr) const;

  uint8_t Read8(uint32_t addr) const;
  uint16_t Read16(uint32_t addr) const;
  uint32_t Read32(uint32_t addr) const;

  void Write8(uint32_t addr, uint8_t v);
  void Write16(uint32_t addr, uint16_t v);
  void Write32(uint32_t addr, uint32_t v);

  // lwl/lwr/swl/swr : accès non alignés du MIPS. Le compilateur Sony les
  // émet pour les copies de chaînes, ils ne sont pas optionnels.
  uint32_t ReadUnaligned32(uint32_t addr) const;
  void WriteUnaligned32(uint32_t addr, uint32_t v);

  uint32_t ram_size() const { return ram_size_; }

 private:
  uint8_t* ram_ = nullptr;
  uint8_t* scratch_ = nullptr;
  uint32_t ram_size_ = 0;
};

}  // namespace psp
