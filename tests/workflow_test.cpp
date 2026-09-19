// Host simulation linked against real upstream MFRC522 and Preferences declarations.
// Does not exercise electrical signaling, RF communication or actual NVS storage.
#include <assert.h>
#include <vector>
#include <functional>
#include "Arduino.h"
#include "SPI.h"
#include "MFRC522.h"
#include "Preferences.h"
FakeSerial Serial;
FakeSPI SPI;
uint32_t fakeMillis = 0;
std::function<void()> onService;
void beginWebControl() {}
void serviceWeb() { if (onService) onService(); }
std::vector<uint8_t> nvs;
unsigned nvsWrites = 0;
struct FakeCard {
  MFRC522::Uid uid;
  uint8_t blocks[64][16];
  int failAuthSector = -1;
  int failReadBlock = -1;
  int failWriteBlock = -1;
  int corruptWriteBlock = -1;
  bool gen1a = false;
  bool directWrite = false;
  bool unlocked = false;
  bool selected = false;
  bool keepUidOnBlock0Write = false;
  bool failAuthAfterBlock0Write = false;
  bool failReadAfterBlock0Write = false;
  bool block0Written = false;
  int authenticatedSector = -1;
  unsigned backdoorCalls = 0;
  unsigned rfResets = 0;
  unsigned normalBlock0ReadsAfterWrite = 0;
  byte readLength = 18;
  std::vector<uint8_t> writes;
} card;
std::function<void()> onBackdoor;

MFRC522::MFRC522(byte, byte) {}
void MFRC522::PCD_Init() {}
void MFRC522::PCD_StopCrypto1() { card.authenticatedSector = -1; }
void MFRC522::PCD_AntennaOff() {
  ++card.rfResets;
  card.unlocked = false; card.selected = false; card.authenticatedSector = -1;
}
void MFRC522::PCD_AntennaOn() {}
byte MFRC522::PCD_ReadRegister(PCD_Register) { return 0x92; }
bool MFRC522::PICC_IsNewCardPresent() { return true; }
bool MFRC522::PICC_ReadCardSerial() {
  assert(!card.unlocked);
  uid = card.uid; card.selected = true; return true;
}
MFRC522::StatusCode MFRC522::PICC_Select(Uid*, byte) { return STATUS_OK; }
MFRC522::StatusCode MFRC522::PICC_HaltA() {
  card.selected = false; card.authenticatedSector = -1; return STATUS_OK;
}
MFRC522::PICC_Type MFRC522::PICC_GetType(byte sak) { return sak == 8 ? PICC_TYPE_MIFARE_1K : PICC_TYPE_UNKNOWN; }
const __FlashStringHelper* MFRC522::GetStatusCodeName(StatusCode) { return reinterpret_cast<const __FlashStringHelper*>("status"); }
const __FlashStringHelper* MFRC522::PICC_GetTypeName(PICC_Type) { return reinterpret_cast<const __FlashStringHelper*>("card type"); }
MFRC522::StatusCode MFRC522::PCD_Authenticate(byte, byte block, MIFARE_Key*, Uid*) {
  assert(card.selected && !card.unlocked);
  if (block / 4 == card.failAuthSector || (card.block0Written && card.failAuthAfterBlock0Write))
    return STATUS_TIMEOUT;
  card.authenticatedSector = block / 4;
  return STATUS_OK;
}
MFRC522::StatusCode MFRC522::MIFARE_Read(byte block, byte* buffer, byte* size) {
  assert(*size >= 18);
  assert(card.unlocked || card.authenticatedSector == block / 4);
  if (block == card.failReadBlock || (card.block0Written && card.failReadAfterBlock0Write))
    return STATUS_TIMEOUT;
  if (block == 0 && card.block0Written && !card.unlocked) ++card.normalBlock0ReadsAfterWrite;
  memcpy(buffer,card.blocks[block],16); *size = card.readLength; return STATUS_OK;
}
bool MFRC522::MIFARE_OpenUidBackdoor(bool) {
  ++card.backdoorCalls;
  assert(card.authenticatedSector == -1);
  if (onBackdoor) onBackdoor();
  card.unlocked = card.gen1a;
  return card.unlocked;
}
MFRC522::StatusCode MFRC522::MIFARE_Write(byte block, byte* data, byte size) {
  assert(block < 64 && block % 4 != 3 && size == 16);
  assert(block == 0 ? (card.gen1a && card.unlocked) ||
      (card.selected && !card.unlocked && card.authenticatedSector == 0) : card.authenticatedSector == block / 4);
  card.writes.push_back(block);
  if (block == 0 && !card.unlocked && !card.directWrite) return STATUS_MIFARE_NACK;
  if (block == card.failWriteBlock) return STATUS_TIMEOUT;
  memcpy(card.blocks[block],data,16);
  if (block == 0) {
    card.block0Written = true;
    if (!card.keepUidOnBlock0Write) memcpy(card.uid.uidByte,data,4);
  }
  if (block == card.corruptWriteBlock) card.blocks[block][block == 0 ? 9 : 0] ^= 1;
  return STATUS_OK;
}
Preferences::Preferences() {}
Preferences::~Preferences() {}
bool Preferences::begin(const char*, bool, const char*) { return true; }
size_t Preferences::putBytes(const char*, const void* data, size_t size) {
  const auto* p = static_cast<const uint8_t*>(data); nvs.assign(p,p+size); ++nvsWrites; return size;
}
size_t Preferences::getBytesLength(const char*) { return nvs.size(); }
size_t Preferences::getBytes(const char*, void* out, size_t capacity) {
  if (capacity < nvs.size()) return 0;
  memcpy(out,nvs.data(),nvs.size()); return nvs.size();
}
bool Preferences::isKey(const char*) { return !nvs.empty(); }
bool Preferences::remove(const char*) { nvs.clear(); return true; }

#include "../CardBackup/CardBackup.ino"

void newCard(uint8_t id, uint8_t dataSeed = 0) {
  card = FakeCard{};
  card.uid.size = 4; card.uid.sak = 8; card.uid.uidByte[0] = id;
  for (unsigned b=0; b<64; ++b) {
    for (unsigned j=0; j<16; ++j) card.blocks[b][j] = static_cast<uint8_t>(dataSeed+b+j);
    if (b%4 == 3) { card.blocks[b][6]=0xFF; card.blocks[b][7]=7; card.blocks[b][8]=0x80; }
  }
  memcpy(card.blocks[0], card.uid.uidByte, 4);
  card.blocks[0][4] = id;
  card.blocks[0][5] = 0x08;
  card.blocks[0][6] = 0x04;
  card.blocks[0][7] = 0x00;
  Serial.output.clear();
}
void writePrepared() { handleCommand("WRITE " + hexBytes(targetUid.uidByte,targetUid.size)); }
void writeBlock0Prepared() {
  handleCommand("WRITE0 " + hexBytes(targetUid.uidByte,targetUid.size) + " " + decimal(confirmationGeneration));
}
void writeCuidPrepared() {
  handleCommand("WRITECUID " + hexBytes(targetUid.uidByte, targetUid.size) + " " + decimal(confirmationGeneration));
}

void testStandardBlock0(const cardbackup::Backup& original) {
  saved = original;
  String error;

  // Automatic preparation writes nothing, even when the ordinary card is read-only.
  newCard(2);
  const auto fixed = card;
  handleCommand("prepare-auto");
  assert(armed && pendingWrite == PendingWrite::Cuid && card.writes.empty());
  assert(card.backdoorCalls == 1 && card.rfResets >= 3);
  assert(controlStateJson().find("\"confirmationKind\":\"cuid\"") != std::string::npos);
  writeCuidPrepared();
  assert(card.writes.size() == 1 && !armed);
  assert(memcmp(card.blocks, fixed.blocks, sizeof(card.blocks)) == 0);
  assert(strcmp(operationPhase, "error") == 0);
  assert(Serial.output.find("BLOCK0 OK") == std::string::npos);

  // Explicit CUID mode never sends a magic command and uses the active authenticated session.
  for (const char* prepare : {"prepare-cuid", "prepare-auto"}) {
    newCard(2, 90); card.directWrite = true;
    const auto before = card;
    handleCommand(prepare);
    assert(armed && pendingWrite == PendingWrite::Cuid && card.writes.empty());
    const unsigned expectedProbeCount = strcmp(prepare, "prepare-auto") == 0 ? 1 : 0;
    assert(card.backdoorCalls == expectedProbeCount);
    writeCuidPrepared();
    assert(card.backdoorCalls == expectedProbeCount && card.writes.size() == 1 && card.writes[0] == 0);
    assert(card.normalBlock0ReadsAfterWrite >= 1 && !card.unlocked && !armed);
    assert(cardbackup::sameUid(card.uid.uidByte, card.uid.size, saved.uid, saved.uidSize));
    for (unsigned b = 0; b < 64; ++b)
      assert(memcmp(card.blocks[b], b == 0 ? saved.blocks[0] : before.blocks[b], 16) == 0);
    assert(strcmp(operationPhase, "success") == 0 && Serial.output.find("BLOCK0 OK") != std::string::npos);
    writeCuidPrepared(); assert(card.writes.size() == 1);
    assert(memcmp(&saved, &original, sizeof(saved)) == 0 && nvsWrites == 1);
  }

  // Gen1A remains preferred in automatic mode; a failed confirmed magic write
  // must not cause an unconfirmed standard write attempt.
  newCard(2); card.gen1a = card.directWrite = true;
  handleCommand("prepare-auto");
  assert(armed && pendingWrite == PendingWrite::Block0 && card.writes.empty());
  card.gen1a = false;
  writeBlock0Prepared();
  assert(!armed && card.writes.empty());

  // Re-selection after a failed probe cannot silently bind another card or changed data.
  for (bool swapUid : {false, true}) {
    newCard(2); card.directWrite = true;
    onBackdoor = [swapUid]() {
      if (swapUid) card.uid.uidByte[0] = 3;
      else card.blocks[0][12] ^= 1;
    };
    handleCommand("prepare-auto"); onBackdoor = nullptr;
    assert(!armed && card.writes.empty());
  }

  for (unsigned reason = 0; reason < 6; ++reason) {
    newCard(2); card.directWrite = true;
    if (reason == 0) card.uid = MFRC522::Uid{4, {1, 0, 0, 0}, 8};
    if (reason == 1) card.failAuthSector = 0;
    if (reason == 2) card.failReadBlock = 0;
    if (reason == 3) card.blocks[3][8] = 0;
    if (reason == 4) card.uid.size = 7;
    if (reason == 5) card.readLength = 16;
    handleCommand("prepare-cuid");
    assert(!armed && card.writes.empty() && card.backdoorCalls == 0);
  }
  for (unsigned reason = 0; reason < 5; ++reason) {
    newCard(2); card.directWrite = true; handleCommand("prepare-cuid"); assert(armed);
    if (reason == 0) card.uid.uidByte[0] = 3;
    if (reason == 1) card.blocks[0][12] ^= 1;
    if (reason == 2) card.blocks[3][8] = 0;
    if (reason == 3) fakeMillis += CONFIRM_TIMEOUT_MS;
    if (reason == 4) { saved.blocks[1][0] ^= 1; cardbackup::seal(saved); }
    writeCuidPrepared(); saved = original;
    assert(!armed && card.writes.empty());
  }

  // Keep all three confirmation modes separate, including forged/replayed API requests.
  newCard(2); card.directWrite = card.gen1a = true;
  handleCommand("prepare-cuid"); writePrepared(); assert(!armed && card.writes.empty());
  handleCommand("prepare-cuid"); writeBlock0Prepared(); assert(!armed && card.writes.empty());
  handleCommand("restore"); writeCuidPrepared(); assert(!armed && card.writes.empty());
  handleCommand("prepare-block0"); writeCuidPrepared(); assert(!armed && card.writes.empty());
  handleCommand("prepare-cuid");
  const String old = "WRITECUID 02000000 " + decimal(confirmationGeneration);
  handleCommand("cancel"); handleCommand("prepare-cuid"); handleCommand(old);
  assert(!armed && card.writes.empty());
  handleCommand("prepare-cuid");
  const String nonce = decimal(confirmationGeneration);
  assert(enqueueWebAction("write", "02000000", nonce, error) == 409);
  assert(enqueueWebAction("write-block0", "02000000", nonce, error) == 409);
  assert(enqueueWebAction("write-cuid", "03000000", nonce, error) == 409);
  assert(enqueueWebAction("write-cuid", "02000000", nonce, error) == 202);
  assert(enqueueWebAction("write-cuid", "02000000", nonce, error) == 409);
  runQueuedCommand();
  assert(card.writes.size() == 1 && !operationBusy && !armed);
  assert(enqueueWebAction("write-cuid", "02000000", nonce, error) == 409);

  // ACK alone, wrong UID, or failed/incorrect read-back never reports success.
  for (unsigned reason = 0; reason < 5; ++reason) {
    newCard(2); card.directWrite = true; handleCommand("prepare-cuid");
    if (reason == 0) card.failWriteBlock = 0;
    if (reason == 1) card.keepUidOnBlock0Write = true;
    if (reason == 2) card.corruptWriteBlock = 0;
    if (reason == 3) card.failAuthAfterBlock0Write = true;
    if (reason == 4) card.failReadAfterBlock0Write = true;
    writeCuidPrepared();
    assert(card.writes.size() == 1 && !armed && strcmp(operationPhase, "error") == 0);
    assert(Serial.output.find("BLOCK0 OK") == std::string::npos);
    assert(card.backdoorCalls == 0);
    assert(memcmp(&saved, &original, sizeof(saved)) == 0 && nvsWrites == 1);
  }
}
void testBlock0(const cardbackup::Backup& original) {
  assert(cardbackup::canCopyBlock0(saved));
  newCard(2);
  handleCommand("prepare-block0");
  assert(!armed && card.writes.empty() && card.backdoorCalls == 1 && !card.unlocked);

  newCard(1); card.gen1a = true;
  handleCommand("prepare-block0");
  assert(!armed && card.writes.empty() && card.backdoorCalls == 0);
  newCard(2); card.gen1a = true; card.uid.size = 7;
  handleCommand("prepare-block0");
  assert(!armed && card.writes.empty() && card.backdoorCalls == 0);

  // Rejected layouts are valid NVS records, so they must fail the additional compatibility check.
  for (unsigned byteIndex : {0u, 4u, 5u, 6u, 7u}) {
    saved = original; saved.blocks[0][byteIndex] ^= 1; cardbackup::seal(saved);
    newCard(2); card.gen1a = true;
    handleCommand("prepare-block0");
    assert(!armed && card.writes.empty() && card.backdoorCalls == 0);
  }
  saved = original;
  for (bool failAuth : {false, true}) {
    newCard(2); card.gen1a = true;
    if (failAuth) card.failAuthSector = 0; else card.failReadBlock = 0;
    handleCommand("prepare-block0");
    assert(!armed && card.writes.empty() && card.backdoorCalls == 0);
  }
  newCard(2); card.gen1a = true; card.readLength = 16;
  handleCommand("prepare-block0");
  assert(!armed && card.writes.empty() && card.backdoorCalls == 0);

  newCard(2); card.gen1a = true;
  handleCommand("prepare-block0");
  assert(armed && pendingWrite == PendingWrite::Block0 && !card.unlocked && card.writes.empty());
  newCard(3); card.gen1a = true;
  writeBlock0Prepared(); assert(!armed && card.writes.empty());

  newCard(2); card.gen1a = true;
  handleCommand("prepare-block0"); saved.blocks[1][2] ^= 1; cardbackup::seal(saved);
  writeBlock0Prepared(); assert(!armed && card.writes.empty());
  saved = original;

  newCard(2); card.gen1a = true;
  handleCommand("prepare-block0"); card.blocks[0][12] ^= 1;
  writeBlock0Prepared(); assert(!armed && card.writes.empty());

  newCard(2); card.gen1a = true;
  handleCommand("prepare-block0"); card.gen1a = false;
  writeBlock0Prepared(); assert(!armed && card.writes.empty() && !card.unlocked);

  newCard(2); card.gen1a = true;
  handleCommand("prepare-block0"); fakeMillis += CONFIRM_TIMEOUT_MS;
  writeBlock0Prepared(); assert(!armed && card.writes.empty());
  newCard(2); card.gen1a = true;
  handleCommand("prepare-block0"); fakeMillis = armedAt + CONFIRM_TIMEOUT_MS - 1;
  writeBlock0Prepared(); assert(!armed && card.writes.empty());

  // A second card with the same UID but different manufacturer bytes must not pass after unlocking.
  newCard(2); card.gen1a = true;
  handleCommand("prepare-block0");
  onBackdoor = []() { card.blocks[0][12] ^= 1; };
  writeBlock0Prepared(); onBackdoor = nullptr;
  assert(!armed && card.writes.empty() && !card.unlocked);

  newCard(2); card.gen1a = true;
  handleCommand("prepare-block0");
  onBackdoor = []() { fakeMillis += CONFIRM_TIMEOUT_MS; };
  writeBlock0Prepared(); onBackdoor = nullptr;
  assert(!armed && card.writes.empty() && !card.unlocked);

  // Each confirmation applies to exactly one write mode, including direct serial commands.
  newCard(2); card.gen1a = true;
  handleCommand("prepare-block0"); writePrepared();
  assert(!armed && card.writes.empty());
  handleCommand("restore"); assert(pendingWrite == PendingWrite::UserData);
  writeBlock0Prepared(); assert(!armed && card.writes.empty());

  String webError;
  handleCommand("prepare-block0");
  const String block0Confirmation = decimal(confirmationGeneration);
  assert(enqueueWebAction("write", "02000000", block0Confirmation, webError) == 409);
  assert(card.writes.empty());
  handleCommand("restore");
  assert(enqueueWebAction("write-block0", "02000000", decimal(confirmationGeneration), webError) == 409);
  assert(card.writes.empty());

  handleCommand("prepare-block0");
  const String staleSerialCommand = "WRITE0 02000000 " + decimal(confirmationGeneration);
  handleCommand("cancel"); handleCommand("prepare-block0");
  handleCommand(staleSerialCommand);
  assert(!armed && card.writes.empty());
  handleCommand("prepare-block0");
  handleCommand("WRITE0 02000000");
  assert(!armed && card.writes.empty());

  // Success requires writing all 16 bytes, then reselecting and reading through normal authentication.
  newCard(2, 90); card.gen1a = true;
  const auto before = card;
  handleCommand("prepare-block0");
  assert(armed && card.writes.empty());
  writeBlock0Prepared();
  assert(!armed && pendingWrite == PendingWrite::None && !card.unlocked);
  assert(card.writes.size() == 1 && card.writes[0] == 0);
  assert(card.normalBlock0ReadsAfterWrite >= 1);
  assert(cardbackup::sameUid(card.uid.uidByte, card.uid.size, saved.uid, saved.uidSize));
  for (unsigned b = 0; b < 64; ++b)
    assert(memcmp(card.blocks[b], b == 0 ? saved.blocks[0] : before.blocks[b], 16) == 0);
  assert(Serial.output.find("BLOCK0 OK") != std::string::npos);
  assert(memcmp(&saved, &original, sizeof(saved)) == 0 && nvsWrites == 1);
  writeBlock0Prepared(); assert(card.writes.size() == 1);
  handleCommand("prepare-block0"); assert(!armed); // Copied UID now equals source; do not repeat.
  assert(card.writes.size() == 1);

  // Never report success for write NAK, wrong UID, altered bytes or failed authenticated read-back.
  for (unsigned failure = 0; failure < 5; ++failure) {
    newCard(2); card.gen1a = true;
    handleCommand("prepare-block0");
    if (failure == 0) card.failWriteBlock = 0;
    if (failure == 1) card.keepUidOnBlock0Write = true;
    if (failure == 2) card.corruptWriteBlock = 0;
    if (failure == 3) card.failAuthAfterBlock0Write = true;
    if (failure == 4) card.failReadAfterBlock0Write = true;
    writeBlock0Prepared();
    assert(!armed && !card.unlocked && card.writes.size() == 1);
    assert(Serial.output.find("BLOCK0 OK") == std::string::npos);
    assert(strcmp(operationPhase, "error") == 0);
    assert(memcmp(&saved, &original, sizeof(saved)) == 0 && nvsWrites == 1);
    writeBlock0Prepared(); assert(card.writes.size() == 1);
  }

  newCard(2); card.gen1a = true;
  assert(enqueueWebAction("prepare-block0", "", "", webError) == 202);
  assert(operationBusy && card.writes.empty()); runQueuedCommand();
  assert(armed && pendingWrite == PendingWrite::Block0);
  const String oldConfirmation = decimal(confirmationGeneration);
  handleCommand("cancel"); handleCommand("prepare-block0");
  assert(oldConfirmation != decimal(confirmationGeneration));
  assert(enqueueWebAction("write-block0", "02000000", oldConfirmation, webError) == 409);
  assert(enqueueWebAction("write-block0", "03000000", decimal(confirmationGeneration), webError) == 409);
  assert(card.writes.empty());
  const String activeConfirmation = decimal(confirmationGeneration);
  assert(enqueueWebAction("write-block0", "02000000", activeConfirmation, webError) == 202);
  assert(enqueueWebAction("write-block0", "02000000", activeConfirmation, webError) == 409);
  runQueuedCommand();
  assert(!operationBusy && !armed && card.writes.size() == 1);
  assert(Serial.output.find("BLOCK0 OK") != std::string::npos);
  assert(enqueueWebAction("write-block0", "02000000", activeConfirmation, webError) == 409);
  assert(memcmp(&saved, &original, sizeof(saved)) == 0 && nvsWrites == 1);
}
int main() {
  setup();
  assert(!hasBackup);
  newCard(1,10);
  handleCommand("backup");
  assert(hasBackup && cardbackup::valid(saved) && nvsWrites == 1 && card.writes.empty());
  const auto original = saved;
  // Power-cycle state loads the complete persistent backup.
  memset(&saved,0,sizeof(saved)); hasBackup=false; setup();
  assert(hasBackup && memcmp(&saved,&original,sizeof(saved))==0);
  newCard(2,99); card.failAuthSector=8;
  handleCommand("backup");
  assert(memcmp(&saved,&original,sizeof(saved))==0 && nvsWrites==1);
  newCard(2,99); card.failReadBlock=21;
  handleCommand("backup");
  assert(memcmp(&saved,&original,sizeof(saved))==0 && nvsWrites==1);
  newCard(1);
  handleCommand("restore"); assert(!armed && card.writes.empty());
  newCard(2);
  handleCommand("WRITE 02000000"); assert(card.writes.empty());
  handleCommand("restore"); assert(armed);
  handleCommand("cancel"); writePrepared(); assert(card.writes.empty());
  handleCommand("restore"); fakeMillis += CONFIRM_TIMEOUT_MS; writePrepared(); assert(card.writes.empty());
  handleCommand("restore"); assert(armed); card.uid.uidByte[0]=3;
  writePrepared(); assert(card.writes.empty());
  newCard(2); card.blocks[23][8]=0;
  handleCommand("restore"); assert(!armed && card.writes.empty());
  newCard(2); handleCommand("restore"); assert(armed);
  card.blocks[23][8]=0; writePrepared(); assert(card.writes.empty());
  newCard(2); handleCommand("restore"); assert(armed);
  const auto before = card;
  writePrepared(); assert(card.writes.size()==47 && !armed);
  for (unsigned b=0;b<64;++b) {
    const auto* expected=cardbackup::isUserBlock(b) ? saved.blocks[b] : before.blocks[b];
    assert(memcmp(card.blocks[b],expected,16)==0);
  }
  assert(Serial.output.find("RESTORE OK")!=std::string::npos);
  Serial.output.clear(); handleCommand("verify");
  assert(Serial.output.find("VERIFY OK")!=std::string::npos);
  // First write failure stops further writes and is never reported as success.
  newCard(2); handleCommand("restore"); card.failWriteBlock=6;
  writePrepared(); assert(card.writes.back()==6 && card.writes.size()==5);
  assert(Serial.output.find("STOPPED")!=std::string::npos);
  assert(Serial.output.find("RESTORE OK")==std::string::npos);
  // A successful write ACK with incorrect stored data is also detected.
  newCard(2); handleCommand("restore"); card.corruptWriteBlock=5;
  writePrepared(); assert(card.writes.back()==5 && card.writes.size()==4);
  assert(Serial.output.find("verify mismatch")!=std::string::npos);
  newCard(2); card.uid.sak=0;
  handleCommand("restore"); assert(!armed && card.writes.empty());
  assert(memcmp(&saved,&original,sizeof(saved))==0);

  // Web requests are queued; repeated requests cannot touch the reader twice.
  String webError;
  newCard(2);
  assert(enqueueWebAction("restore", "", "", webError)==202);
  assert(operationBusy && !armed && card.writes.empty());
  assert(enqueueWebAction("restore", "", "", webError)==409);
  runQueuedCommand();
  assert(armed && !operationBusy);
  const String oldConfirmation = decimal(confirmationGeneration);
  handleCommand("cancel");
  handleCommand("restore");
  assert(armed && oldConfirmation != decimal(confirmationGeneration));
  assert(enqueueWebAction("write", "02000000", oldConfirmation, webError)==409);
  assert(enqueueWebAction("write", "03000000", decimal(confirmationGeneration), webError)==409);
  assert(card.writes.empty());

  // Status polling must not cancel confirmation; a serial action must cancel it.
  assert(controlStateJson().find("\"armed\":true")!=std::string::npos);
  assert(armed);
  handleCommand("info");
  assert(!armed);
  handleCommand("restore");
  const String activeConfirmation = decimal(confirmationGeneration);
  assert(enqueueWebAction("write", "02000000", activeConfirmation, webError)==202);
  assert(enqueueWebAction("write", "02000000", activeConfirmation, webError)==409);
  unsigned serviced = 0;
  onService = [&]() {
    ++serviced;
    assert(operationBusy);
    assert(enqueueWebAction("erase-backup", "", "ERASE", webError)==409);
    assert(controlStateJson().find("\"busy\":true")!=std::string::npos);
  };
  runQueuedCommand();
  onService = nullptr;
  assert(serviced >= 47 && card.writes.size()==47 && !armed && !operationBusy);
  assert(enqueueWebAction("write", "02000000", activeConfirmation, webError)==409);
  assert(controlStateJson().find("\"phase\":\"success\"")!=std::string::npos);

  // Page refresh cannot revive an expired write, and unknown actions are refused.
  handleCommand("restore");
  fakeMillis += CONFIRM_TIMEOUT_MS;
  assert(controlStateJson().find("\"armed\":false")!=std::string::npos);
  assert(!armed);
  assert(enqueueWebAction("unsupported", "", "", webError)==400);
  assert(enqueueWebAction("erase-backup", "", "", webError)==400);
  assert(hasBackup);
  assert(jsonString("a\"b\\c\n")=="\"a\\\"b\\\\c\\u000a\"");
  const String exported = backupJson();
  assert(exported.find("mifare-classic-1k-user-data-v1")!=std::string::npos);
  assert(exported.find("\"63\":")==std::string::npos);
  assert(exported.find("\"62\":")!=std::string::npos);
  testBlock0(original);
  testStandardBlock0(original);
  nvs[20]^=1; hasBackup=false; setup(); assert(!hasBackup);
  puts("PASS: persistent reload, incomplete reads, source refusal, confirmation/cancel/expiry/card swap, access preflight, 47 verified writes, protected-block preservation, write/readback failures, unsupported card, corrupt NVS");
  puts("PASS: web queue, duplicate requests, stale confirmation, busy exclusion, polling during writes, JSON export");
  puts("PASS: Gen1A-only block 0 preparation, layout checks, source/swap/expiry/mode refusal, one exact write, authenticated read-back, failures and single-use web confirmation");
  puts("PASS: automatic read-only selection, RF reset/reselection, standard authenticated writes, fixed-card NAK, mode isolation, nonce/replay protection, no retry or fallback after confirmed write");
}
