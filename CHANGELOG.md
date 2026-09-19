# Changelog

## v0.1.0 - 2026-09-20

### Added
- Add the standalone ESP32-S3 Wi-Fi access point, captive portal, and Chinese web interface for card identification, backup, restore, verification, and JSON export.
- Add explicit, single-use confirmation flows for Gen1A and standard-authenticated CUID block 0 writes, with target re-selection and full read-back verification.
- Add host workflow, backup-format, and browser UI tests covering persistence, protected blocks, confirmation isolation, failure handling, and responsive web controls.
- Add full setup documentation and a migration handoff covering verified hardware behavior, known card results, and remaining validation work.

### Changed
- Exclude Arduino build output, local archives, Python caches, and macOS metadata from source control.
