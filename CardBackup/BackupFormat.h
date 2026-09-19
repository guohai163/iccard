#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cardbackup {
constexpr uint32_t MAGIC = 0x31424349; // ICB1, little endian
constexpr uint16_t FORMAT_VERSION = 1;
constexpr unsigned BLOCK_COUNT = 64;
constexpr unsigned USER_BLOCK_COUNT = 47;

struct Backup {
  uint32_t magic;
  uint16_t version;
  uint8_t uidSize;
  uint8_t sak;
  uint8_t uid[10];
  uint8_t reserved[2];
  // Block 0 is excluded from normal restore; the explicit Gen1A flow validates it.
  // Trailer entries are always zero, not recovered keys.
  uint8_t blocks[BLOCK_COUNT][16];
  uint32_t crc;
};
static_assert(offsetof(Backup, crc) == 1044, "Backup layout changed");
static_assert(sizeof(Backup) == 1048, "Backup layout changed");

inline bool isUserBlock(unsigned block) {
  return block > 0 && block < BLOCK_COUNT && block % 4 != 3;
}

inline uint32_t crc32(const void* data, size_t size) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  uint32_t crc = 0xFFFFFFFF;
  while (size--) {
    crc ^= *bytes++;
    for (unsigned i = 0; i < 8; ++i) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0u);
    }
  }
  return ~crc;
}

inline void seal(Backup& backup) {
  backup.crc = crc32(&backup, offsetof(Backup, crc));
}

inline bool valid(const Backup& backup) {
  return backup.magic == MAGIC && backup.version == FORMAT_VERSION &&
         (backup.uidSize == 4 || backup.uidSize == 7) &&
         backup.crc == crc32(&backup, offsetof(Backup, crc));
}

// Conservative Gen1A layout: four-byte UID + BCC, SAK 08, ATQA 04 00.
// Some real manufacturers use different byte layouts; do not write those blindly.
inline bool canCopyBlock0(const Backup& backup) {
  if (!valid(backup) || backup.uidSize != 4 || backup.sak != 0x08 ||
      backup.uid[0] == 0x88 || memcmp(backup.uid, backup.blocks[0], 4) != 0) return false;
  const uint8_t* block = backup.blocks[0];
  return block[4] == static_cast<uint8_t>(block[0] ^ block[1] ^ block[2] ^ block[3]) &&
         block[5] == 0x08 && block[6] == 0x04 && block[7] == 0x00;
}

inline bool sameUid(const uint8_t* a, uint8_t aSize,
                    const uint8_t* b, uint8_t bSize) {
  return aSize == bSize && aSize > 0 && aSize <= 10 &&
         memcmp(a, b, aSize) == 0;
}

inline bool factoryAccess(const uint8_t* trailer) {
  return trailer[6] == 0xFF && trailer[7] == 0x07 && trailer[8] == 0x80;
}
} // namespace cardbackup
