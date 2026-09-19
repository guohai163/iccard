// ESP32-S3 + MFRC522: identify and back up authorized MIFARE Classic 1K cards.
// Library: MFRC522 by GithubCommunity, version 1.4.12.
// Serial Monitor: 115200 baud, Newline (or Both NL & CR).
#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include "Config.h"
#include "BackupFormat.h"
#include "WebControl.h"

MFRC522 reader(PIN_SS, PIN_RST);
Preferences storage;
cardbackup::Backup saved = {};
bool storageReady = false;
bool hasBackup = false;
bool readerReady = false;
bool armed = false;
enum class PendingWrite { None, UserData, Block0, Cuid };
PendingWrite pendingWrite = PendingWrite::None;
MFRC522::Uid targetUid = {};
uint8_t preparedTargetBlock0[16] = {};
uint32_t preparedSourceCrc = 0;
uint32_t armedAt = 0;
String input;
bool inputOverflow = false;
bool operationBusy = false;
String queuedCommand;
const char* operationPhase = "idle";
String operationMessage = "已就绪，请将一张卡贴近读卡器。";
String operationDetail;
String operationDiagnostics;
String lastCardUid;
String lastCardType;
unsigned operationProgress = 0;
unsigned operationTotal = 0;
uint32_t confirmationGeneration = 0;

void reportOperation(const char* phase, const String& message,
                     unsigned progress = 0, unsigned total = 0) {
  operationPhase = phase;
  operationMessage = message;
  operationProgress = progress;
  operationTotal = total;
}

String decimal(unsigned long value) {
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%lu", value);
  return String(buffer);
}

String jsonString(const String& value) {
  String result = "\"";
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if (c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
    else if (c < 32) {
      char escaped[7];
      snprintf(escaped, sizeof(escaped), "\\u%04x", c);
      result += escaped;
    } else result += static_cast<char>(c);
  }
  result += '"';
  return result;
}

String hexBytes(const uint8_t* bytes, size_t size) {
  const char* digits = "0123456789ABCDEF";
  String result;
  result.reserve(size * 2);
  for (size_t i = 0; i < size; ++i) {
    result += digits[bytes[i] >> 4];
    result += digits[bytes[i] & 15];
  }
  return result;
}

void recordDiagnostic(const char* stage, const char* outcome, const String& detail) {
  const String line = "[" + String(stage) + "] " + outcome + ": " + detail;
  Serial.println(line);
  // Bound the diagnostic payload and RAM use; a normal operation is well below this.
  if (operationDiagnostics.length() + line.length() + 1 <= 4096) {
    operationDiagnostics += line;
    operationDiagnostics += '\n';
  }
}

void finishCard() {
  reader.PICC_HaltA();
  reader.PCD_StopCrypto1();
}

void disarmWrite() {
  armed = false;
  pendingWrite = PendingWrite::None;
}

void closeMagicSession() {
  // Gen1A special-mode probing must never leave the card unlocked.
  reader.PCD_StopCrypto1();
  reader.PCD_AntennaOff();
  delay(30);
  reader.PCD_AntennaOn();
  delay(30);
}

void printError(const char* operation, uint8_t block, MFRC522::StatusCode code) {
  Serial.printf("ERROR: %s, block %u: ", operation, block);
  Serial.println(reader.GetStatusCodeName(code));
  operationDetail = String(operation) + ", block " + decimal(block) + ": " +
                    String(reader.GetStatusCodeName(code));
}

void printCard() {
  Serial.print("UID: ");
  Serial.println(hexBytes(reader.uid.uidByte, reader.uid.size));
  Serial.printf("UID bytes: %u, SAK: %02X\n", reader.uid.size, reader.uid.sak);
  Serial.print("Type (inferred from SAK): ");
  Serial.println(reader.PICC_GetTypeName(reader.PICC_GetType(reader.uid.sak)));
  lastCardUid = hexBytes(reader.uid.uidByte, reader.uid.size);
  lastCardType = String(reader.PICC_GetTypeName(reader.PICC_GetType(reader.uid.sak)));
}

bool selectCard(bool require1K) {
  if (!readerReady) {
    Serial.println("Reader unavailable. Check 3.3 V and SPI wiring, then reset.");
    reportOperation("error", "读卡器未就绪，请检查 3.3V 供电与接线后重启。 ");
    return false;
  }
  // Reset the RF field so a card halted by an earlier command can be selected again.
  reader.PCD_StopCrypto1();
  reader.PCD_AntennaOff();
  delay(30);
  reader.PCD_AntennaOn();
  delay(30);
  Serial.println("Place ONE card on the reader (15 s timeout)...");
  reportOperation("waiting", "请将一张卡贴近读卡器，等待识别（最多 15 秒）。");
  const uint32_t start = millis();
  while (millis() - start < CARD_TIMEOUT_MS) {
    serviceWeb(); // Keep status polling responsive while waiting for a card.
    if (reader.PICC_IsNewCardPresent() && reader.PICC_ReadCardSerial()) {
      printCard();
      if (require1K &&
          (reader.PICC_GetType(reader.uid.sak) != MFRC522::PICC_TYPE_MIFARE_1K ||
           (reader.uid.size != 4 && reader.uid.size != 7))) {
        Serial.println("This command supports MIFARE Classic 1K (S50), 4/7-byte UID only.");
        reportOperation("error", "当前卡型不支持备份或恢复，请使用 M1 / S50（1K）卡。");
        finishCard();
        return false;
      }
      return true;
    }
    delay(50);
  }
  reader.PCD_StopCrypto1();
  Serial.println("No card selected. Check placement/type/wiring.");
  reportOperation("error", "未检测到卡片，请调整位置后重试。");
  return false;
}

bool authenticate(uint8_t sector, bool source) {
  MFRC522::MIFARE_Key key;
  memcpy(key.keyByte, source ? SOURCE_KEYS[sector] : TARGET_KEY, 6);
  const uint8_t command = source && SOURCE_USE_KEY_B[sector]
      ? MFRC522::PICC_CMD_MF_AUTH_KEY_B : MFRC522::PICC_CMD_MF_AUTH_KEY_A;
  const auto status = reader.PCD_Authenticate(command, sector * 4 + 3, &key, &reader.uid);
  if (status != MFRC522::STATUS_OK) {
    printError("authenticate", sector * 4 + 3, status);
    Serial.printf("Sector %u needs the correct key and read/write permissions.\n", sector);
    return false;
  }
  return true;
}

bool readBlock(uint8_t block, uint8_t* out) {
  uint8_t buffer[18]; // 16 data bytes + CRC_A, as required by MFRC522.
  uint8_t length = sizeof(buffer);
  const auto status = reader.MIFARE_Read(block, buffer, &length);
  if (status != MFRC522::STATUS_OK) {
    printError("read", block, status);
    return false;
  }
  if (length != sizeof(buffer)) {
    Serial.printf("ERROR: short read, block %u: received %u bytes, expected 18\n", block, length);
    operationDetail = "Short read at block " + decimal(block);
    return false;
  }
  memcpy(out, buffer, 16);
  return true;
}

bool readTargetBlock0WithDiagnostics(uint8_t* out) {
  const bool authenticated = authenticate(0, false);
  recordDiagnostic("AUTH0", authenticated ? "OK" : "FAILED",
                   authenticated ? String("Sector 0 authenticated with configured Key A") : operationDetail);
  if (!authenticated) return false;
  const bool readable = readBlock(0, out);
  recordDiagnostic("READ0/AUTHENTICATED", readable ? "OK" : "FAILED",
                   readable ? hexBytes(out, 16) : operationDetail);
  return readable;
}

bool checkStandardBlock0Access() {
  // This checks the known-key transport configuration, not manufacturer-block
  // writability. Ordinary fixed-UID cards can pass this read-only check too.
  uint8_t trailer[16];
  if (!readBlock(3, trailer)) {
    recordDiagnostic("STANDARD/ACCESS", "FAILED", operationDetail);
    return false;
  }
  if (!cardbackup::factoryAccess(trailer)) {
    operationDetail = "Target sector 0 is not in the supported FF0780 access configuration";
    recordDiagnostic("STANDARD/ACCESS", "FAILED", operationDetail);
    return false;
  }
  recordDiagnostic("STANDARD/ACCESS", "OK", "Authentication/read/access checks passed; block 0 writability NOT tested");
  return true;
}

bool checkExistingGen1AWithDiagnostics(const uint8_t* expected) {
  reader.PCD_StopCrypto1();
  recordDiagnostic("GEN1A/HANDSHAKE", "BEGIN", "Using the existing MFRC522 library; serial details enabled");
  // Keep the existing protocol and acceptance rules. The flag only enables the
  // library's serial error messages identifying the failed handshake sub-stage.
  const bool supported = reader.MIFARE_OpenUidBackdoor(true);
  if (!supported) {
    operationDetail = "Gen1A handshake failed; serial output identifies the failed sub-stage";
    recordDiagnostic("GEN1A/HANDSHAKE", "FAILED", operationDetail);
    return false;
  }
  recordDiagnostic("GEN1A/HANDSHAKE", "OK", "Library handshake completed");
  uint8_t probed[16];
  const bool readable = readBlock(0, probed);
  recordDiagnostic("READ0/GEN1A", readable ? "OK" : "FAILED",
                   readable ? hexBytes(probed, 16) : operationDetail);
  if (!readable) return false;
  if (memcmp(probed, expected, 16) != 0) {
    operationDetail = "Gen1A block 0 differs from the authenticated read";
    recordDiagnostic("COMPARE0/GEN1A", "FAILED", "Expected " + hexBytes(expected, 16) + "; received " + hexBytes(probed, 16));
    return false;
  }
  recordDiagnostic("COMPARE0/GEN1A", "OK", "All 16 bytes match the authenticated read");
  return true;
}

void showStatus() {
  Serial.printf("Reader: %s; NVS storage: %s\n",
                readerReady ? "ready" : "unavailable", storageReady ? "ready" : "unavailable");
  if (!hasBackup) {
    Serial.println("No valid saved backup.");
    return;
  }
  Serial.print("Saved source UID: ");
  Serial.println(hexBytes(saved.uid, saved.uidSize));
  Serial.printf("47 user blocks (752 bytes), plus block 0 metadata. CRC32: %08lX\n",
                static_cast<unsigned long>(saved.crc));
  Serial.println("Normal restore copies 47 user blocks only. Block 0 uses a separate confirmed Gen1A or standard-authenticated write.");
}

void backupCard() {
  if (!storageReady) {
    Serial.println("NVS unavailable; backup cancelled.");
    reportOperation("error", "存储不可用，无法保存备份。");
    return;
  }
  if (!selectCard(true)) return;
  cardbackup::Backup candidate = {};
  candidate.magic = cardbackup::MAGIC;
  candidate.version = cardbackup::FORMAT_VERSION;
  candidate.uidSize = reader.uid.size;
  candidate.sak = reader.uid.sak;
  memcpy(candidate.uid, reader.uid.uidByte, reader.uid.size);
  bool ok = true;
  for (uint8_t sector = 0; sector < 16 && ok; ++sector) {
    reportOperation("reading", "正在读取原卡，请保持卡片不动。", sector, 16);
    serviceWeb();
    ok = authenticate(sector, true);
    for (uint8_t offset = 0; offset < 3 && ok; ++offset) {
      const uint8_t block = sector * 4 + offset;
      ok = readBlock(block, candidate.blocks[block]);
    }
    if (ok) Serial.printf("Read sector %u/15\n", sector);
  }
  finishCard();
  if (!ok) {
    Serial.println("Backup incomplete; previous saved backup kept. Source was not written.");
    reportOperation("error", "备份未完成，已保留之前的备份。请检查卡片位置或扇区密钥。");
    return;
  }
  cardbackup::seal(candidate);
  if (storage.putBytes("backup", &candidate, sizeof(candidate)) != sizeof(candidate)) {
    Serial.println("ERROR: could not save backup to NVS.");
    reportOperation("error", "写入板上存储失败，请重试。");
    return;
  }
  cardbackup::Backup check = {};
  if (storage.getBytes("backup", &check, sizeof(check)) != sizeof(check) ||
      !cardbackup::valid(check) || memcmp(&candidate, &check, sizeof(check)) != 0) {
    Serial.println("ERROR: NVS read-back failed; new backup not accepted.");
    reportOperation("error", "板上存储校验失败，本次备份未确认成功。");
    return;
  }
  saved = candidate;
  hasBackup = true;
  Serial.println("BACKUP OK: saved to flash; survives reset/power loss.");
  reportOperation("success", "备份成功，47 个用户数据块已保存，断电后仍保留。", 16, 16);
  showStatus();
}

bool requireBackup() {
  if (hasBackup && cardbackup::valid(saved)) return true;
  Serial.println("No valid backup. Run backup with your source card first.");
  reportOperation("error", "还没有有效备份，请先备份原卡。");
  return false;
}

bool isSourceCard() {
  return cardbackup::sameUid(saved.uid, saved.uidSize, reader.uid.uidByte, reader.uid.size);
}

bool preflightTarget() {
  // Complete all-sector permission checks before the first write.
  for (uint8_t sector = 0; sector < 16; ++sector) {
    reportOperation("checking", "正在检查目标卡的 16 个扇区。", sector, 16);
    serviceWeb();
    uint8_t trailer[16];
    if (!authenticate(sector, false) || !readBlock(sector * 4 + 3, trailer)) return false;
    if (!cardbackup::factoryAccess(trailer)) {
      Serial.printf("Sector %u is not in factory access configuration (FF0780).\n", sector);
      operationDetail = "Sector " + decimal(sector) + ": access bits are not FF0780";
      return false;
    }
  }
  return true;
}

void prepareRestore() {
  if (!requireBackup() || !selectCard(true)) return;
  if (isSourceCard()) {
    Serial.println("REFUSED: target UID equals source UID. Use a separate blank card.");
    reportOperation("error", "这是原卡，请移走原卡并换上另一张空白卡。");
    finishCard();
    return;
  }
  const bool ok = preflightTarget();
  targetUid = reader.uid;
  finishCard();
  if (!ok) {
    Serial.println("Preflight failed; nothing written.");
    reportOperation("error", "目标卡检查未通过，没有写入。请使用出厂配置的空白 M1 卡。");
    return;
  }
  armed = true;
  pendingWrite = PendingWrite::UserData;
  armedAt = millis();
  ++confirmationGeneration;
  reportOperation("confirm", "目标卡检查通过。请核对卡号，并在倒计时结束前确认写入。");
  Serial.println("Will overwrite 47 user blocks on this target. Keep ONLY this card in place.");
  Serial.print("Within 30 seconds send: WRITE ");
  Serial.println(hexBytes(targetUid.uidByte, targetUid.size));
  Serial.println("Any other command cancels. UID and sector trailers will remain unchanged.");
}

void restoreCard(const String& confirmation) {
  const bool permitted = armed && pendingWrite == PendingWrite::UserData &&
      millis() - armedAt < CONFIRM_TIMEOUT_MS &&
      confirmation == "WRITE " + hexBytes(targetUid.uidByte, targetUid.size);
  disarmWrite(); // Confirmation is single-use, including failed attempts.
  if (!permitted || !requireBackup()) {
    Serial.println("Not confirmed or expired. Run restore again.");
    reportOperation("error", "确认无效或已过期，请重新检查目标卡。");
    return;
  }
  if (!selectCard(true)) return;
  if (millis() - armedAt >= CONFIRM_TIMEOUT_MS || isSourceCard() ||
      !cardbackup::sameUid(targetUid.uidByte, targetUid.size,
                           reader.uid.uidByte, reader.uid.size)) {
    Serial.println("Target changed, source detected, or confirmation expired; nothing written.");
    reportOperation("error", "目标卡已更换或确认已过期，没有写入。请重新检查目标卡。");
    finishCard();
    return;
  }
  if (!preflightTarget()) {
    Serial.println("Preflight failed; nothing written.");
    reportOperation("error", "目标卡检查未通过，没有写入。");
    finishCard();
    return;
  }
  unsigned verified = 0;
  bool ok = true;
  for (uint8_t sector = 0; sector < 16 && ok; ++sector) {
    ok = authenticate(sector, false);
    for (uint8_t offset = 0; offset < 3 && ok; ++offset) {
      const uint8_t block = sector * 4 + offset;
      if (!cardbackup::isUserBlock(block)) continue;
      reportOperation("writing", "正在写入并回读校验，请保持卡片不动、供电稳定。", verified, 47);
      serviceWeb();
      const auto status = reader.MIFARE_Write(block, saved.blocks[block], 16);
      if (status != MFRC522::STATUS_OK) {
        printError("write", block, status);
        ok = false;
        break;
      }
      uint8_t actual[16];
      ok = readBlock(block, actual);
      if (ok && memcmp(actual, saved.blocks[block], 16) != 0) {
        Serial.printf("ERROR: verify mismatch at block %u\n", block);
        operationDetail = "Read-back mismatch at block " + decimal(block);
        ok = false;
      }
      if (ok) ++verified;
    }
  }
  finishCard();
  if (ok && verified == cardbackup::USER_BLOCK_COUNT) {
    Serial.println("RESTORE OK: 47 user blocks written and verified. UID/keys/access NOT copied.");
    reportOperation("success", "恢复成功，47 / 47 个用户数据块已写入并校验。", 47, 47);
  } else {
    Serial.printf("STOPPED: %u/47 blocks verified. Target may be partially written; no rollback.\n", verified);
    Serial.println("Saved source backup is intact. Fix the problem, then run restore again.");
    reportOperation("error", "写入中止，目标卡可能只写入了部分数据。板上备份仍保留，可排除问题后重新恢复。", verified, 47);
  }
}

bool requireBlock0Backup() {
  if (!requireBackup()) return false;
  if (cardbackup::canCopyBlock0(saved)) {
    recordDiagnostic("SOURCE0", "OK", "Backup integrity and block 0 layout validated");
    return true;
  }
  recordDiagnostic("SOURCE0", "FAILED", "Backup UID/BCC/SAK/ATQA layout is not supported");
  Serial.println("BLOCK0 REFUSED: requires a valid 4-byte UID/S50 backup with matching UID, BCC, SAK and ATQA.");
  reportOperation("error", "备份的第 0 块不适用于此流程。需要 4 字节 UID、有效校验字节及兼容的 S50 布局。");
  return false;
}

void prepareManufacturerBlock(bool tryGen1A, bool allowStandard) {
  if (!requireBlock0Backup() || !selectCard(true)) return;
  if (reader.uid.size != 4 || isSourceCard()) {
    Serial.println("BLOCK0 REFUSED: use a separate 4-byte UID target, not the source UID.");
    reportOperation("error", "请移走原卡，只放一张不同卡号的 4 字节 UID 目标卡。");
    finishCard();
    return;
  }
  uint8_t original[16];
  if (!readTargetBlock0WithDiagnostics(original)) {
    finishCard();
    reportOperation("error", "无法读取目标卡第 0 块，没有写入。请检查位置及第 0 扇区默认密钥。");
    return;
  }
  if (memcmp(original, reader.uid.uidByte, 4) != 0 ||
      original[4] != static_cast<uint8_t>(original[0] ^ original[1] ^ original[2] ^ original[3])) {
    recordDiagnostic("TARGET0/UID-BCC", "FAILED", "Block 0 does not match the selected UID or BCC");
    finishCard();
    Serial.println("BLOCK0 REFUSED: target block 0 does not match its selected UID/BCC.");
    reportOperation("error", "目标卡的第 0 块与实时卡号不一致，没有写入。");
    return;
  }
  recordDiagnostic("TARGET0/UID-BCC", "OK", "Block 0 matches the selected UID and BCC");
  const MFRC522::Uid selected = reader.uid;
  bool magicVerified = false;
  if (tryGen1A) {
    finishCard();
    reportOperation("checking", "正在检查 UID / Gen1A 方式，此步骤只读取，不写卡。");
    serviceWeb();
    magicVerified = checkExistingGen1AWithDiagnostics(original);
    closeMagicSession();
    recordDiagnostic("RF/CLEANUP", "OK", "RF field restarted after Gen1A check; no block written");
    if (!magicVerified && !allowStandard) {
      Serial.println("BLOCK0 UNSUPPORTED: Gen1A probe/read-back failed; no block was written.");
      reportOperation("error", "Gen1A 检查未通过，没有写入。可以查看诊断或重新选择自动 / CUID 模式。");
      return;
    }
    if (!magicVerified) {
      // A HALT alone would leave the card sleeping. selectCard() resets the RF
      // field, then performs fresh anticollision before ordinary authentication.
      recordDiagnostic("STANDARD/FALLBACK", "BEGIN", "Gen1A not confirmed; reset RF, reselect and authenticate; no writes during preparation");
      if (!selectCard(true)) return;
      if (!cardbackup::sameUid(selected.uidByte, selected.size, reader.uid.uidByte, reader.uid.size)) {
        finishCard();
        recordDiagnostic("STANDARD/TARGET", "FAILED", "Card changed during preparation");
        reportOperation("error", "检查期间卡片发生变化，没有写入。请只放一张目标卡后重试。");
        return;
      }
      uint8_t reselected[16];
      if (!readTargetBlock0WithDiagnostics(reselected) || memcmp(reselected, original, 16) != 0) {
        finishCard();
        recordDiagnostic("STANDARD/TARGET", "FAILED", "Read failed or original block 0 changed after RF reset");
        reportOperation("error", "重新读取失败或第 0 块发生变化，没有写入。");
        return;
      }
    }
  }
  if (!magicVerified) {
    if (!checkStandardBlock0Access()) {
      finishCard();
      reportOperation("error", "普通认证 / 访问条件检查未通过，没有写入。查看详细诊断后重试。");
      return;
    }
    operationDetail = ""; // Earlier Gen1A failure is still preserved in diagnostics.
    recordDiagnostic("STANDARD/PREPARED", "UNVERIFIED", "Read-only preparation complete; no WRITE command sent. CUID/OTP/fixed UID cannot be distinguished by this check");
  }
  finishCard();
  targetUid = selected;
  memcpy(preparedTargetBlock0, original, sizeof(original));
  preparedSourceCrc = saved.crc;
  armed = true;
  pendingWrite = magicVerified ? PendingWrite::Block0 : PendingWrite::Cuid;
  armedAt = millis();
  ++confirmationGeneration;
  reportOperation("confirm", magicVerified
      ? "Gen1A 读取验证通过。确认后将覆盖第 0 块的完整 16 字节，请只放目标卡。"
      : "普通认证和读取已通过，但尚未确认第 0 块可写。确认后才会实际尝试标准写入；一次性卡可能写后锁定。");
  Serial.println(magicVerified ? "Selected write method: Gen1A (read-back verified)"
                             : "Selected write method: STANDARD/CUID-compatible; writability and exact card type remain UNKNOWN");
  Serial.print("Current target UID: "); Serial.println(hexBytes(targetUid.uidByte, targetUid.size));
  Serial.print("Replacement UID: "); Serial.println(hexBytes(saved.uid, saved.uidSize));
  Serial.print("Current target block 0: "); Serial.println(hexBytes(preparedTargetBlock0, 16));
  Serial.print("Replacement block 0: "); Serial.println(hexBytes(saved.blocks[0], 16));
  Serial.print(magicVerified ? "Within 30 seconds send: WRITE0 " : "Within 30 seconds send: WRITECUID ");
  Serial.print(hexBytes(targetUid.uidByte, targetUid.size));
  Serial.print(' '); Serial.println(decimal(confirmationGeneration));
  Serial.println("Keep ONLY the target in the RF field. Other commands cancel. One-time cards may lock after a real write.");
}

void prepareBlock0() { prepareManufacturerBlock(true, false); }
void prepareAutoBlock0() { prepareManufacturerBlock(true, true); }
void prepareCuidBlock0() { prepareManufacturerBlock(false, true); }

void writeManufacturerBlock(const String& confirmation, bool standardWrite) {
  const PendingWrite expectedKind = standardWrite ? PendingWrite::Cuid : PendingWrite::Block0;
  const String prefix = standardWrite ? "WRITECUID " : "WRITE0 ";
  const bool permitted = armed && pendingWrite == expectedKind &&
      millis() - armedAt < CONFIRM_TIMEOUT_MS &&
      confirmation == prefix + hexBytes(targetUid.uidByte, targetUid.size) + " " + decimal(confirmationGeneration);
  disarmWrite();
  if (!permitted) {
    Serial.println("BLOCK0 REFUSED: confirmation expired, wrong mode or wrong token. Prepare the target again.");
    reportOperation("error", "第 0 块确认已过期、类型不符或确认码错误，请重新检查。");
    return;
  }
  if (!requireBlock0Backup()) return;
  if (saved.crc != preparedSourceCrc) {
    Serial.println("BLOCK0 REFUSED: source backup changed since preparation.");
    reportOperation("error", "准备后原卡备份发生变化，没有写入。请重新检查目标卡。");
    return;
  }
  if (!selectCard(true)) return;
  if (millis() - armedAt >= CONFIRM_TIMEOUT_MS || isSourceCard() ||
      !cardbackup::sameUid(targetUid.uidByte, targetUid.size, reader.uid.uidByte, reader.uid.size)) {
    finishCard();
    Serial.println("BLOCK0 REFUSED: target changed or confirmation expired; nothing written.");
    reportOperation("error", "目标卡已更换或确认已过期，没有写入。");
    return;
  }
  uint8_t current[16];
  const bool unchanged = readTargetBlock0WithDiagnostics(current) &&
                         memcmp(current, preparedTargetBlock0, 16) == 0;
  recordDiagnostic("TARGET0/UNCHANGED", unchanged ? "OK" : "FAILED",
                   unchanged ? "Target block 0 still matches the prepared snapshot" : "Target changed or authenticated read failed");
  if (!unchanged) {
    finishCard();
    Serial.println("BLOCK0 REFUSED: target block 0 changed or cannot be read; nothing written.");
    reportOperation("error", "目标卡第 0 块已变化或无法读取，没有写入。请重新检查。");
    return;
  }
  if (standardWrite) {
    // Keep the ordinary authenticated session active until MIFARE_Write.
    // Never call the magic handshake, switch mode, or retry after a write failure.
    if (!checkStandardBlock0Access() || millis() - armedAt >= CONFIRM_TIMEOUT_MS) {
      finishCard();
      reportOperation("error", "访问条件检查失败或确认已过期，没有写入。");
      return;
    }
    recordDiagnostic("STANDARD/WRITE0", "CONFIRMED", "One real standard write using the active Key A authenticated session");
  } else {
    finishCard();
    reportOperation("checking", "再次核对目标卡的 Gen1A 支持与第 0 块内容。", 0, 1);
    serviceWeb();
    // Revalidate the prepared method; do not change to standard write here.
    if (!checkExistingGen1AWithDiagnostics(preparedTargetBlock0) ||
        millis() - armedAt >= CONFIRM_TIMEOUT_MS) {
      closeMagicSession();
      Serial.println("BLOCK0 REFUSED: final Gen1A check failed or expired; nothing written.");
      reportOperation("error", "最终检查失败或确认已过期，没有写入。请重新检查目标卡。");
      return;
    }
  }
  reportOperation("writing", "正在写入第 0 块。请勿移动卡片或断电。", 0, 1);
  // Exactly one write attempt. A failed ACK may still mean the card was changed.
  const auto status = reader.MIFARE_Write(0, saved.blocks[0], 16);
  recordDiagnostic("WRITE0/ACK", status == MFRC522::STATUS_OK ? "OK" : "FAILED",
                   String(reader.GetStatusCodeName(status)));
  closeMagicSession();
  if (status != MFRC522::STATUS_OK) {
    printError("block0 write", 0, status);
    Serial.println("BLOCK0 UNCONFIRMED: write failed; target may already have changed. No automatic retry.");
    reportOperation("error", standardWrite && status == MFRC522::STATUS_MIFARE_NACK
        ? "标准写入被卡片拒绝，可能是固定 UID、锁定或访问限制；不能据此认定厂家或卡型。请重新识别，程序不会自动重试。"
        : "第 0 块写入结果未确认，卡片可能已修改。请保持供电并重新识别；程序不会自动重试。");
    return;
  }
  reportOperation("verifying", "正在重新识别新卡号，并回读第 0 块。", 0, 1);
  if (!selectCard(true)) {
    Serial.println("BLOCK0 UNCONFIRMED: card could not be reselected after writing.");
    reportOperation("error", "写入后无法重新识别，卡片可能已修改，结果未确认。请保持供电并检查卡片位置。");
    return;
  }
  if (!cardbackup::sameUid(saved.uid, saved.uidSize, reader.uid.uidByte, reader.uid.size)) {
    recordDiagnostic("VERIFY/UID", "FAILED", "Expected " + hexBytes(saved.uid, saved.uidSize) +
                     "; received " + hexBytes(reader.uid.uidByte, reader.uid.size));
    finishCard();
    Serial.println("BLOCK0 UNCONFIRMED: selected UID does not equal the source UID after writing.");
    reportOperation("error", "重新识别的卡号与原卡不一致，第 0 块写入未确认成功。");
    return;
  }
  recordDiagnostic("VERIFY/UID", "OK", hexBytes(reader.uid.uidByte, reader.uid.size));
  const bool verified = authenticate(0, false) && readBlock(0, current) &&
                        memcmp(current, saved.blocks[0], 16) == 0;
  recordDiagnostic("VERIFY/BLOCK0", verified ? "OK" : "FAILED",
                   verified ? "All 16 bytes match the source backup" : "Authenticated read failed or data differs");
  finishCard();
  if (!verified) {
    Serial.println("BLOCK0 UNCONFIRMED: UID selected, but full block 0 read-back failed or differs.");
    reportOperation("error", "新卡号已识别，但第 0 块完整回读校验未通过。卡片已可能修改，请勿当作成功结果。");
    return;
  }
  Serial.print("BLOCK0 OK: selected UID "); Serial.print(hexBytes(reader.uid.uidByte, reader.uid.size));
  Serial.println(" and all 16 bytes verified. Sector keys/access conditions were not copied.");
  recordDiagnostic("WRITE0/METHOD", "VERIFIED", standardWrite
      ? "Standard authenticated write verified; this does not prove repeatability or exact chip type"
      : "Gen1A write verified");
  reportOperation("success", "第 0 块写入成功：新卡号已变为 " + hexBytes(reader.uid.uidByte, reader.uid.size) +
                  "，完整 16 字节回读一致。可继续识别或校验用户数据。", 1, 1);
}

void writeBlock0(const String& confirmation) { writeManufacturerBlock(confirmation, false); }
void writeCuidBlock0(const String& confirmation) { writeManufacturerBlock(confirmation, true); }

void verifyCard() {
  if (!requireBackup() || !selectCard(true)) return;
  bool ok = true;
  unsigned matches = 0;
  unsigned checked = 0;
  for (uint8_t sector = 0; sector < 16 && ok; ++sector) {
    ok = authenticate(sector, false);
    for (uint8_t offset = 0; offset < 3 && ok; ++offset) {
      const uint8_t block = sector * 4 + offset;
      if (!cardbackup::isUserBlock(block)) continue;
      reportOperation("verifying", "正在比较目标卡与备份的用户数据。", checked, 47);
      serviceWeb();
      uint8_t actual[16];
      ok = readBlock(block, actual);
      if (ok && memcmp(actual, saved.blocks[block], 16) == 0) ++matches;
      else if (ok) Serial.printf("Different data at block %u\n", block);
      if (ok) ++checked;
    }
  }
  finishCard();
  Serial.printf("%s: %u/47 user blocks match. UID/keys/access excluded.\n",
                ok && matches == cardbackup::USER_BLOCK_COUNT ? "VERIFY OK" : "VERIFY FAILED", matches);
  reportOperation(ok && matches == 47 ? "success" : "error",
                  ok && matches == 47 ? String("校验通过，47 / 47 个用户数据块完全一致。") :
                  "校验未通过，匹配 " + decimal(matches) + " / 47 块。请检查目标卡或重新恢复。",
                  checked, 47);
}

String backupJson() {
  String json;
  json.reserve(2300);
  json = "{\"format\":\"mifare-classic-1k-user-data-v1\",\"uid\":\"";
  json += hexBytes(saved.uid, saved.uidSize);
  json += "\",\"sak\":" + decimal(saved.sak) + ",\"blocks\":{";
  bool first = true;
  for (unsigned block = 0; block < cardbackup::BLOCK_COUNT; ++block) {
    if (block != 0 && !cardbackup::isUserBlock(block)) continue;
    if (!first) json += ',';
    json += "\"" + decimal(block) + "\":\"" + hexBytes(saved.blocks[block], 16) + "\"";
    first = false;
  }
  json += "}}";
  return json;
}

void dumpBackup() {
  if (!requireBackup()) return;
  Serial.println("BEGIN BACKUP JSON (not a full card dump; no key recovery)");
  Serial.println("{\n  \"format\": \"mifare-classic-1k-user-data-v1\",");
  Serial.print("  \"uid\": \"");
  Serial.print(hexBytes(saved.uid, saved.uidSize));
  Serial.printf("\",\n  \"sak\": %u,\n  \"blocks\": {\n", saved.sak);
  bool first = true;
  for (unsigned block = 0; block < cardbackup::BLOCK_COUNT; ++block) {
    if (block != 0 && !cardbackup::isUserBlock(block)) continue;
    if (!first) Serial.println(",");
    Serial.printf("    \"%u\": \"", block);
    Serial.print(hexBytes(saved.blocks[block], 16));
    Serial.print('"');
    first = false;
  }
  Serial.println("\n  }\n}\nEND BACKUP JSON");
  Serial.println("Copy the JSON to your computer. Firmware does not import JSON files.");
}

void help() {
  Serial.println("\nESP32-S3 + RC522 / MIFARE Classic 1K user-data backup");
  Serial.println("help         - commands (115200 baud; Newline)");
  Serial.println("info         - identify one card; read only");
  Serial.println("backup       - read source with configured known keys; replace saved backup on success");
  Serial.println("status       - saved backup information");
  Serial.println("dump         - export saved data as JSON through Serial");
  Serial.println("restore      - check a blank target and request UID confirmation");
  Serial.println("WRITE <UID>  - confirm prepared target; case-sensitive");
  Serial.println("prepare-block0 - check a separate Gen1A target for full block 0 copy; no writes");
  Serial.println("prepare-auto - read-only Gen1A check, then standard-authenticated preparation if needed");
  Serial.println("prepare-cuid - standard Key A authentication/read checks ONLY; no writeability test");
  Serial.println("WRITE0 <UID> <token> - confirm block 0 copy using the exact printed command");
  Serial.println("WRITECUID <UID> <token> - confirm one REAL standard block 0 write; OTP cards may lock");
  Serial.println("verify       - compare target data using factory Key A");
  Serial.println("cancel       - cancel pending write");
  Serial.println("erase-backup - delete the saved backup from this ESP32 only");
  Serial.printf("Web UI       - connect to Wi-Fi %s, open http://192.168.4.1/\n", WIFI_AP_SSID);
}

void executeCommand(String command) {
  command.trim();
  if (command.length() == 0) return;
  if (command.startsWith("WRITECUID ")) {
    writeCuidBlock0(command);
    return;
  }
  if (command.startsWith("WRITE0 ")) {
    writeBlock0(command);
    return;
  }
  if (command.startsWith("WRITE ")) {
    restoreCard(command);
    return;
  }
  disarmWrite();
  if (command == "help") help();
  else if (command == "info") {
    if (selectCard(false)) {
      finishCard();
      reportOperation("success", "识别成功，请查看最近识别的卡片。");
    }
  }
  else if (command == "backup") backupCard();
  else if (command == "status") showStatus();
  else if (command == "dump") dumpBackup();
  else if (command == "restore") prepareRestore();
  else if (command == "prepare-block0") prepareBlock0();
  else if (command == "prepare-auto") prepareAutoBlock0();
  else if (command == "prepare-cuid") prepareCuidBlock0();
  else if (command == "verify") verifyCard();
  else if (command == "cancel") {
    Serial.println("Pending write cancelled.");
    reportOperation("idle", "已取消待确认的写入。");
  }
  else if (command == "erase-backup") {
    if (storageReady && (!storage.isKey("backup") || storage.remove("backup"))) {
      memset(&saved, 0, sizeof(saved));
      hasBackup = false;
      Serial.println("Saved backup erased. No card was written.");
      reportOperation("success", "已删除板上的备份。");
    } else {
      Serial.println("ERROR: could not erase saved backup.");
      reportOperation("error", "删除备份失败。");
    }
  } else {
    Serial.println("Unknown command. Send help.");
    reportOperation("error", "未知命令，请发送 help 查看帮助。");
  }
}

void handleCommand(String command) {
  command.trim();
  if (command.length() == 0) return;
  if (operationBusy) {
    Serial.println("BUSY: wait for the current operation to finish.");
    return;
  }
  operationBusy = true;
  operationDetail = "";
  operationDiagnostics = "";
  reportOperation("working", "正在处理操作…");
  executeCommand(command);
  if (strcmp(operationPhase, "working") == 0) reportOperation("idle", "已就绪。");
  operationBusy = false;
}

void expireConfirmation() {
  if (armed && millis() - armedAt >= CONFIRM_TIMEOUT_MS) {
    disarmWrite();
    Serial.println("Write confirmation expired. Nothing written.");
    reportOperation("expired", "确认已过期，没有写入。请重新检查目标卡。");
  }
}

bool controlBusy() { return operationBusy; }
bool controlHasBackup() { return hasBackup && cardbackup::valid(saved); }

String controlStateJson() {
  expireConfirmation();
  String json;
  json.reserve(1500 + operationDiagnostics.length());
  json = "{\"busy\":";
  json += operationBusy ? "true" : "false";
  json += ",\"readerReady\":"; json += readerReady ? "true" : "false";
  json += ",\"storageReady\":"; json += storageReady ? "true" : "false";
  json += ",\"hasBackup\":"; json += controlHasBackup() ? "true" : "false";
  json += ",\"phase\":" + jsonString(operationPhase);
  json += ",\"message\":" + jsonString(operationMessage);
  json += ",\"detail\":" + jsonString(operationDetail);
  json += ",\"diagnostics\":" + jsonString(operationDiagnostics);
  json += ",\"cardUid\":" + jsonString(lastCardUid);
  json += ",\"cardType\":" + jsonString(lastCardType);
  json += ",\"sourceUid\":" + jsonString(hasBackup ? hexBytes(saved.uid, saved.uidSize) : String(""));
  char crc[9];
  snprintf(crc, sizeof(crc), "%08lX", (unsigned long)saved.crc);
  json += ",\"crc\":" + jsonString(hasBackup ? String(crc) : String(""));
  json += ",\"progress\":" + decimal(operationProgress);
  json += ",\"total\":" + decimal(operationTotal);
  json += ",\"armed\":"; json += armed ? "true" : "false";
  json += ",\"block0Eligible\":"; json += hasBackup && cardbackup::canCopyBlock0(saved) ? "true" : "false";
  json += ",\"confirmationKind\":" + jsonString(armed ?
      (pendingWrite == PendingWrite::Block0 ? "block0" : pendingWrite == PendingWrite::Cuid ? "cuid" : "data") : "");
  json += ",\"block0Mode\":" + jsonString(armed ?
      (pendingWrite == PendingWrite::Block0 ? "Gen1A" : pendingWrite == PendingWrite::Cuid ? "CUID" : "") : "");
  json += ",\"replacementUid\":" + jsonString(hasBackup ? hexBytes(saved.uid, saved.uidSize) : String(""));
  json += ",\"sourceBlock0\":" + jsonString(hasBackup ? hexBytes(saved.blocks[0], 16) : String(""));
  json += ",\"targetUid\":" + jsonString(armed ? hexBytes(targetUid.uidByte, targetUid.size) : String(""));
  json += ",\"confirmation\":" + jsonString(armed ? decimal(confirmationGeneration) : String(""));
  const uint32_t elapsed = millis() - armedAt;
  json += ",\"remainingMs\":" + decimal(armed && elapsed < CONFIRM_TIMEOUT_MS ? CONFIRM_TIMEOUT_MS - elapsed : 0);
  json += ",\"ssid\":" + jsonString(WIFI_AP_SSID) + "}";
  return json;
}

int enqueueWebAction(const String& action, const String& uid,
                     const String& confirmation, String& error) {
  expireConfirmation();
  if (operationBusy) { error = "设备正在操作，请等待完成。"; return 409; }
  String command;
  if (action == "write" || action == "write-block0" || action == "write-cuid") {
    const PendingWrite expected = action == "write" ? PendingWrite::UserData :
        action == "write-block0" ? PendingWrite::Block0 : PendingWrite::Cuid;
    if (!armed || pendingWrite != expected || confirmation != decimal(confirmationGeneration) ||
        uid != hexBytes(targetUid.uidByte, targetUid.size)) {
      error = "写入确认已过期或目标已改变，请重新检查目标卡。";
      return 409;
    }
    command = action == "write" ? "WRITE " + uid :
        (action == "write-block0" ? String("WRITE0 ") : String("WRITECUID ")) + uid + " " + confirmation;
  } else if (action == "erase-backup") {
    if (confirmation != "ERASE") { error = "请确认删除备份。"; return 400; }
    command = action;
  } else if (action == "info" || action == "backup" || action == "restore" || action == "prepare-block0" ||
             action == "prepare-auto" || action == "prepare-cuid" ||
             action == "verify" || action == "cancel") command = action;
  else { error = "不支持的操作。"; return 400; }
  // Reserve the single operation slot before returning the HTTP response.
  queuedCommand = command;
  operationBusy = true;
  operationDetail = "";
  reportOperation("queued", "操作已接收，正在开始…");
  return 202;
}

void runQueuedCommand() {
  if (queuedCommand.length() == 0) return;
  const String command = queuedCommand;
  queuedCommand = "";
  operationBusy = false;
  handleCommand(command);
}

void setup() {
  Serial.begin(115200);
  const uint32_t start = millis();
  while (!Serial && millis() - start < 3000) delay(10);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SS);
  reader.PCD_Init();
  delay(50);
  const uint8_t version = reader.PCD_ReadRegister(MFRC522::VersionReg);
  readerReady = version != 0x00 && version != 0xFF;
  Serial.printf("\nMFRC522 VersionReg: 0x%02X (usually 0x91/0x92; compatible chips may differ)\n", version);
  storageReady = storage.begin("iccard", false);
  if (storageReady && storage.getBytesLength("backup") == sizeof(saved)) {
    hasBackup = storage.getBytes("backup", &saved, sizeof(saved)) == sizeof(saved) &&
                cardbackup::valid(saved);
  }
  input.reserve(80);
  help();
  showStatus();
  beginWebControl();
}

void loop() {
  serviceWeb();
  expireConfirmation();
  runQueuedCommand();
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      if (inputOverflow) {
        disarmWrite();
        Serial.println("Command too long; discarded and pending write cancelled.");
      } else handleCommand(input);
      input = "";
      inputOverflow = false;
    } else if (!inputOverflow && input.length() < 80) input += c;
    else inputOverflow = true;
  }
  delay(5);
}
