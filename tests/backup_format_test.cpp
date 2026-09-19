#include "../CardBackup/BackupFormat.h"
#include <assert.h>
#include <stdio.h>

int main() {
  using namespace cardbackup;
  // Published CRC-32/ISO-HDLC check vector; validates persistence integrity logic.
  assert(crc32("123456789", 9) == 0xCBF43926u);
  assert(crc32("", 0) == 0u);
  unsigned count = 0;
  for (unsigned block = 0; block < 256; ++block) {
    if (isUserBlock(block)) {
      ++count;
      assert(block != 0 && block < 64 && block % 4 != 3);
    }
  }
  assert(count == 47);
  assert(!isUserBlock(0) && !isUserBlock(3) && !isUserBlock(63) && !isUserBlock(256));

  Backup backup = {};
  backup.magic = MAGIC;
  backup.version = FORMAT_VERSION;
  backup.uidSize = 4;
  backup.uid[0] = 0x12;
  seal(backup);
  assert(valid(backup));
  // Every single-bit corruption in the stored record must be rejected.
  for (size_t i = 0; i < sizeof(Backup); ++i) {
    for (unsigned bit = 0; bit < 8; ++bit) {
      Backup corrupt = backup;
      reinterpret_cast<uint8_t*>(&corrupt)[i] ^= 1u << bit;
      assert(!valid(corrupt));
    }
  }
  backup.uidSize = 7;
  seal(backup);
  assert(valid(backup));
  backup.uidSize = 11;
  seal(backup);
  assert(!valid(backup));
  backup.uidSize = 4;
  backup.version = 2;
  seal(backup);
  assert(!valid(backup));

  const uint8_t uid[] = {1, 2, 3, 4, 5, 6, 7};
  const uint8_t other[] = {1, 2, 3, 5};
  assert(sameUid(uid, 4, uid, 4));
  assert(!sameUid(uid, 4, uid, 7));
  assert(!sameUid(uid, 4, other, 4));
  assert(!sameUid(uid, 0, uid, 0));
  assert(!sameUid(uid, 11, uid, 11));

  uint8_t trailer[16] = {};
  trailer[6] = 0xFF; trailer[7] = 0x07; trailer[8] = 0x80;
  assert(factoryAccess(trailer));
  trailer[8] = 0x00;
  assert(!factoryAccess(trailer));

  // A valid persistent backup alone does not make its manufacturer block safe to copy.
  Backup source = {};
  source.magic = MAGIC;
  source.version = FORMAT_VERSION;
  source.uidSize = 4;
  source.sak = 0x08;
  const uint8_t sourceUid[] = {0xAB, 0x0A, 0x1E, 0xBB};
  memcpy(source.uid, sourceUid, sizeof(sourceUid));
  memcpy(source.blocks[0], sourceUid, sizeof(sourceUid));
  source.blocks[0][4] = 0x04;
  source.blocks[0][5] = 0x08;
  source.blocks[0][6] = 0x04;
  source.blocks[0][7] = 0x00;
  for (unsigned i = 8; i < 16; ++i) source.blocks[0][i] = i * 17;
  seal(source);
  assert(valid(source) && canCopyBlock0(source));
  // CRC is resealed so each rejection exercises block-0 compatibility, not corruption detection.
  for (unsigned i = 0; i < 8; ++i) {
    Backup bad = source;
    bad.blocks[0][i] ^= 1;
    seal(bad);
    assert(valid(bad) && !canCopyBlock0(bad));
  }
  Backup bad = source;
  bad.uidSize = 7; seal(bad);
  assert(valid(bad) && !canCopyBlock0(bad));
  bad = source; bad.sak = 0x18; seal(bad);
  assert(valid(bad) && !canCopyBlock0(bad));
  bad = source; bad.uid[0] = bad.blocks[0][0] = 0x88;
  bad.blocks[0][4] = bad.uid[0] ^ bad.uid[1] ^ bad.uid[2] ^ bad.uid[3];
  seal(bad);
  assert(valid(bad) && !canCopyBlock0(bad));
  bad = source; bad.crc ^= 1;
  assert(!canCopyBlock0(bad));
  puts("PASS: CRC vectors, 8384 corruption cases, protected blocks, UID comparison, target access checks");
  puts("PASS: block 0 eligibility requires valid record, four-byte UID, matching UID/BCC and S50 layout");
}
