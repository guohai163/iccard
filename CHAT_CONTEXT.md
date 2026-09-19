# ESP32-S3 + RC522：会话总结与换机交接

整理日期：2026-09-20（Asia/Shanghai）。

本文总结本次会话的需求、实现、实机结果、排查结论和后续工作。它不是逐字聊天记录。适合迁移到新电脑后，直接交给新的开发助手阅读；当前功能以项目实际源码及 `README.md` 为准，较早的聊天结论可能已过时。

## 1. 当前状态：先看这一节

用户有一块 ESP32-S3 开发板和一个 RC522 读卡器，想备份自己的 IC 卡并写入新卡。项目已从串口程序发展为“Wi-Fi 热点 + 中文网页 + 串口”的 Arduino 工程。

**当前源码已经支持 UID / Gen1A 和 CUID 标准认证两种第 0 块写法，也支持自动选择待尝试的写法。不要再沿用早期“CUID 尚未实现”的说明。**

目前证据的边界：

| 项目 | 状态 |
| --- | --- |
| ESP32-S3 编译、烧录、USB 串口 | 用户已经实机验证 |
| RC522 识别普通 M1 / S50 卡 | 用户已经实机验证 |
| 原卡 47 个用户数据块备份、保存到 NVS | 用户已经实机验证 |
| 普通恢复、47/47 数据块回读校验 | 用户已经实机验证 |
| Gen1A 握手检查及诊断 | 用户实机执行过；已有目标卡均在第一步超时 |
| 第 0 块 / UID 写入成功 | **尚无用户提供的实机成功记录** |
| 最新自动 / CUID 模式 | 已实现、通过 ESP32-S3 编译、主机模拟测试和浏览器模拟交互；尚待实体卡验证 |
| Gen1B、Gen3、Gen4 / USCUID 等其他特殊协议 | 当前没有实现 |
| JSON 备份导入 | 当前没有实现，只支持导出 |
| 手机公交卡业务卡号读取 | 讨论过可行性，当前没有实现 |

下一项最有价值的实际工作：在新电脑恢复开发环境，保留板上原卡备份，用最新程序的 `prepare-auto` 或 `prepare-cuid` 检查目标卡，再根据用户明确确认后的实际日志继续排查。不能把模拟成功当成这几张实体卡能够修改 UID 的证明。

## 2. 换电脑时要带走什么

旧机项目目录：

```text
/Users/guohai/Develop/github/iccard
```

建议直接复制整个项目目录，并同时保留原卡备份附件：

```text
/Users/guohai/Downloads/ic-back.json
```

可选保留的原始硬件照片：

```text
/Users/guohai/Downloads/IMG_5013.HEIC
/Users/guohai/Downloads/IMG_5014.HEIC
```

`iccard-starter.zip` 是此前已生成的完整源码包，包含主程序、网页代码、测试和 README，不包含板上 NVS 实时数据。本交接文档是在该源码包之后新生成的，需要一并携带。Downloads 下的附件也没有自动装入源码包。

### Git 迁移注意

项目已整理为 `v0.1.0` 发布，完整源码、网页文件、测试、README、CHANGELOG 和本交接文档应当都在 Git 中。换机后优先克隆远端仓库，再单独带走不入库的原卡备份附件。

整理发布前的仓库基线是：

- 分支：`main`，跟踪 `origin/main`。
- 本地 HEAD：`043fb51`，提交说明 `first commit`，时间 `2026-09-20T07:08:10+08:00`。
- 发布前以下内容曾是未跟踪文件/目录，现已纳入 `v0.1.0` 的内容：

```text
CardBackup/WebControl.cpp
CardBackup/WebControl.h
CardBackup/WebPage.h
tests/
```

`iccard-starter.zip`、`CardBackup/build/` 和 `.DS_Store` 属于归档或生成文件，已通过 `.gitignore` 排除。若远端已经包含 `v0.1.0` 标签，只克隆远端仓库即可取得完整源代码；`ic-back.json` 仍需单独迁移，不能从 Git 恢复。

`CardBackup/build/` 是生成产物，不应当作最新源码的依据。新电脑可以重新编译；不要依赖旧机 `/private/tmp/` 下的测试缓存和固件文件。

### 板上备份与电脑文件是两件事

- 原卡备份保存在 ESP32 的 NVS Flash 上，换电脑、重启和普通断电不影响它。
- 升级程序时保持原来的分区方案，`Erase All Flash Before Sketch Upload` 保持 `Disabled`。
- 全片擦除、改变分区、执行 `erase-backup`，或对另一张卡成功执行 `backup`，都可能丢掉/覆盖原卡备份。
- 电脑上的 JSON 当前不能直接导回板子。若板上备份丢失，现有程序需要重新读取原卡；或者另行开发、验证 JSON 导入功能。

## 3. 硬件确认与接线

用户最初称“ESP32-S3E + RC552”；照片中的读卡模块为 RC522 常见 8 针接口板，板上有 `MH-ET LIVE V2.0` 标识。

开发板有两个 Type-C 接口。用户成功烧录的实际日志：

```text
Chip type: ESP32-S3 (QFN56), revision v0.2
Embedded PSRAM: 8MB (AP_3v3)
USB mode: USB-Serial/JTAG
MFRC522 VersionReg: 0x92
```

`VersionReg: 0x92` 是读卡器芯片的版本，不能拿来判断卡片生产商。

接线按 GPIO 丝印，不按排针的物理序号：

| RC522 | ESP32-S3 |
| --- | --- |
| 3V3 | 3V3 |
| GND | GND |
| SDA / SS | GPIO10 |
| MOSI | GPIO11 |
| SCK | GPIO12 |
| MISO | GPIO13 |
| RST | GPIO14 |
| IRQ | 不接 |

读卡器用 3.3V；RC522 的 `SDA` 在本项目中是 SPI 片选，`RST` 接 GPIO14，而不是开发板的复位引脚。

## 4. 开发环境和烧录配置

已经使用并成功编译的版本：

- Arduino IDE 2。
- `esp32 by Espressif Systems`：**3.3.11**。
- `MFRC522 by GithubCommunity`：**1.4.12**。
- `SPI`、`Preferences`、`WiFi`、`WebServer`、`DNSServer` 随 ESP32 开发板包提供。

ESP32 开发板管理器地址：

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

当前成功配置：

| 选项 | 配置 |
| --- | --- |
| Board | ESP32S3 Dev Module |
| CPU Frequency | 240MHz |
| Flash Mode | QIO 80MHz |
| Flash Size | 已验证的编译配置为 4MB；实际芯片容量以硬件规格为准 |
| Partition Scheme | Default 4MB with spiffs |
| PSRAM | Disabled；当前程序不依赖 PSRAM |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | **Enabled**，用于当前的原生 USB 接口 |
| Upload Mode | UART0 / Hardware CDC |
| Upload Speed | 115200 起步；用户后续也使用过 921600 |
| Erase All Flash Before Sketch Upload | **Disabled** |
| 串口监视器 | 115200，Newline 或 Both NL & CR |

照片中模块的 N16R8 标识曾用于建议 16MB 配置，但本项目实际通过编译和实机普通流程的是默认 4MB 分区配置。为保留 NVS，换机时优先沿用原来可用的分区，不要因为本次迁移自行重分区。

旧机端口曾出现 `/dev/cu.usbmodem1101` 和 `/dev/cu.usbmodem101`；新电脑上的名字可能不同，按插入设备后出现的端口选择，不要硬编码旧名字。

### 已解决的串口无响应问题

用户最初上传成功，但发送 `help` 无输出。实际检查 Arduino 编译缓存发现：

```text
CDCOnBoot=default
ARDUINO_USB_CDC_ON_BOOT=0
```

此时程序的 `Serial` 指向 UART0，而用户连接的是原生 USB。改为 `USB CDC On Boot = Enabled` 并重新编译上传后，串口正常。只改菜单、不重新烧录不会改变已经运行的固件。

上传停在 `Connecting...` 时，可按住 BOOT，按一下并松开 RST，再松开 BOOT后重新上传。上传后按 RST，必要时重新选择枚举后的端口。

## 5. 当前工程文件与结构

下面路径均相对于项目根目录，方便换电脑后定位：

| 文件 | 用途 |
| --- | --- |
| `CardBackup/CardBackup.ino` | 串口命令、卡片读写、NVS、操作状态、确认与网页命令队列 |
| `CardBackup/Config.h` | GPIO、各扇区已知密钥、目标密钥、超时、热点名称与密码 |
| `CardBackup/BackupFormat.h` | 备份结构、CRC32、用户块判断、第 0 块兼容性检查 |
| `CardBackup/WebControl.h` | 网页控制与主程序之间的接口声明 |
| `CardBackup/WebControl.cpp` | Wi-Fi AP、DNS、HTTP API、下载和请求令牌 |
| `CardBackup/WebPage.h` | 完整中文网页，HTML/CSS/JavaScript 内嵌在固件中 |
| `tests/workflow_test.cpp` | 卡片/NVS 模拟与操作流程测试 |
| `tests/backup_format_test.cpp` | 备份格式、CRC、块范围与兼容布局测试 |
| `tests/host/` | 主机测试使用的 Arduino/SPI 替身 |
| `tests/web_ui_test.mjs` | 网页脚本的模拟 DOM/API 测试 |
| `tests/web_preview.py` | 只使用模拟卡片的本地网页预览服务 |
| `tests/run_host_tests.sh` | C++ 主机测试入口 |
| `README.md` | 完整使用手册 |
| `iccard-starter.zip` | 已打包的完整源码，不含真实卡片 NVS 数据 |

网页没有 CDN 或在线字体依赖。HTTP 操作先排队，由 Arduino 主循环执行；读写过程会处理状态请求，但忙碌时拒绝新的冲突操作。网页请求使用每次启动生成的令牌，不另设网页账号。

## 6. Wi-Fi 网页与串口操作

热点参数：

```text
SSID: ICCard-S3
Password: iccard2026
URL: http://192.168.4.1/
```

手机提示此 Wi-Fi 没有互联网时，选择继续连接。该热点用于本地控制，不需要家中路由器或互联网。可用 USB 充电器/移动电源给开发板供电。

网页支持识别、备份、恢复、用户数据校验、下载 JSON、删除板上备份、查看诊断，以及第 0 块写入方式选择。界面有“已保存的原卡备份”和“最近读取的卡片”两栏，含义不同。

串口命令：

| 命令 | 含义 |
| --- | --- |
| `help` | 显示帮助 |
| `info` | 实时识别天线上的卡，读取 UID / SAK / 推断类型 |
| `backup` | 读取原卡；全部成功才覆盖板上现有备份 |
| `status` | 显示板上已有备份的信息 |
| `dump` | 导出板上已有备份为 JSON |
| `restore` | 检查另一张目标卡，准备写入 47 个用户数据块 |
| `WRITE <UID>` | 确认上述普通恢复 |
| `verify` | 从实体卡重新读取 47 个用户数据块并与备份比较 |
| `prepare-auto` | 先做 Gen1A 读取验证；失败则重新选卡，做标准认证读取准备 |
| `prepare-block0` | 保留为仅 Gen1A 的第 0 块检查 |
| `prepare-cuid` | 仅普通认证、读取和访问位检查，不发送后门指令 |
| `WRITE0 <UID> <token>` | 确认 Gen1A 第 0 块写入 |
| `WRITECUID <UID> <token>` | 确认一次普通认证后的第 0 块标准写入 |
| `cancel` | 取消待确认写入 |
| `erase-backup` | 删除板上保存的备份 |

等待放卡最多 15 秒，写入确认有效期 30 秒。`token` 是设备当次打印的确认码，不是固定数字，必须按实际提示发送。三种确认 `WRITE` / `WRITE0` / `WRITECUID` 不能混用；网页会自动使用正确的本次确认信息。

**重要：`dump`、下载 JSON、`status` 不会重新读取当前实体卡。** 它们读取 ESP32 中的 `saved` 备份对象。只想看当前卡号，请使用 `info` 或“识别卡片”；不要对新卡执行 `backup` 后误覆盖原卡备份。

## 7. 卡片与备份：已知的真实数据

### 原卡与附件

原卡：

```text
UID: AB0A1EBB
UID bytes: 4
SAK: 08
Type: MIFARE 1KB
Block 0: AB0A1EBB0408040003C234747F4CA890
Saved backup CRC32: CE4107FF
```

用户提供的 `ic-back.json` 已检查：

- `format`：`mifare-classic-1k-user-data-v1`。
- `uid`：`AB0A1EBB`；`sak`：十进制 `8`。
- 导出 48 个块：第 0 块，以及 47 个用户数据块。
- **47 个用户数据块的每一字节全部是 `00`**，共 752 字节。
- 第 3、7、11……63 块属于扇区尾块，没有导出，不包含完整密钥和访问条件备份。
- 按当前备份布局重建记录计算的 CRC32 为 `CE4107FF`，与用户先前板上备份日志一致。

因此，如果目标卡用户区本来就全零，执行普通恢复后肉眼看不出数据变化是正常的；`47/47` 只表示这些块与备份一致。

### 出现过的卡号与诊断

| UID | 已知角色 / 结果 |
| --- | --- |
| `AB0A1EBB` | 原卡 / 保存的源备份；对同 UID 发起目标写入会被程序保护性拒绝 |
| `9EBA0902` | 用户确认是套装附带的卡；普通读写成功，Gen1A 第一个握手阶段超时 |
| `84F85005` | 曾作为目标卡；普通恢复成功，早期 Gen1A 检查未通过，具体失败阶段未单独提供 |
| `7C0A77A6` | 后续测试卡；普通认证/读 0 块成功，Gen1A 第一个握手阶段超时 |

已读取到的目标第 0 块：

```text
9EBA0902: 9EBA09022F0804006263646566676869
7C0A77A6: 7C0A77A6A708040003FEEB18AB55D81D
```

相应 BCC 为 `2F` 与 `A7`，都匹配前四字节的异或值。`84F85005` 没有提供完整第 0 块，不能自行补造。

### 普通备份、恢复已经成功

用户多次提供真实串口日志：

```text
BACKUP OK: saved to flash; survives reset/power loss.
RESTORE OK: 47 user blocks written and verified. UID/keys/access NOT copied.
VERIFY OK: 47/47 user blocks match. UID/keys/access excluded.
```

这些记录证实用户数据读写正常，但不证明 UID、第 0 块、扇区密钥或门禁系统认可结果已经复制成功。

### Gen1A 检查的实际失败点

`9EBA0902` 和 `7C0A77A6` 的详细日志均显示：

```text
[AUTH0] OK
[READ0/AUTHENTICATED] OK
[TARGET0/UID-BCC] OK
[GEN1A/HANDSHAKE] BEGIN
Card did not respond to 0x40 after HALT command.
Error name: Timeout in communication.
[GEN1A/HANDSHAKE] FAILED
[RF/CLEANUP] OK
BLOCK0 UNSUPPORTED
```

准确结论：本次没有通过 Gen1A 握手，准备阶段没有写第 0 块。不能只凭这条日志就断言卡是正品、永久不可写、一定是 CUID，或完全排除特殊指令时序/兼容性问题。先前聊天中“这一定不是接线或距离问题”“卡一定没有后门”等表述过于绝对，后续应以上述限定结论为准。

Gen1B 也依赖对第一条唤醒指令的响应；因此，仅省掉第二阶段不能自动解决目前第一阶段的超时。当前源码没有增加 Gen1B 支持。

## 8. 第 0 块双模式的实现与边界

支持范围：4 字节 UID 的 MIFARE Classic 1K / S50 兼容目标。源备份需通过 CRC 校验，UID 与第 0 块一致，BCC 正确，并满足本程序保守支持的 `08 04 00` 布局。不是所有厂商的第 0 块都能统一按这个布局处理。

普通恢复始终只写 47 个用户数据块。第 0 块使用独立流程：

1. 读取并核对目标 UID、原始第 0 块，拒绝与源卡相同 UID 的目标。
2. 自动方式先调用已有库的 Gen1A 检查，并在特殊模式下读取、比对完整第 0 块。
3. 如果未确认 Gen1A，重启射频场，重新选择同一张卡，再执行普通认证读取和出厂访问位检查。
4. 显示本次待尝试的写法、目标原 UID、源 UID、完整待写入数据，等待明确确认。
5. 确认绑定目标 UID、写入模式、源备份 CRC、目标原始第 0 块、有效期和当次确认码。
6. 实际写入前重新检查目标。Gen1A 写法重新验证相同的后门方式；CUID 写法保持普通认证会话有效后调用标准块写入。
7. 只尝试一次写入。失败时不自动重试，也不在确认后静默切换其他写法。
8. 写后重新开启射频并选卡，确认 UID 已变为原卡 UID，正常认证回读完整 16 字节后才输出 `BLOCK0 OK`。

必须保留的技术认识：

- `PICC_HaltA()` 是休眠，不是复位。切换方式时通过关闭/重开射频场和重新选卡清理状态。
- 后门 ACK 不是完整验证，当前 Gen1A 流程还要求第 0 块回读一致。
- CUID 准备仅认证和读取成功，**不代表已证实可写或已确认是 CUID 卡**。
- 标准写入是实际修改，不能用“写回原值”或试写来冒充无损检测。一次性 / OTP 卡可能因一次实际写入而锁定。
- NAK 只表示写入被拒绝，不能据此认定生产商、正品身份或确切原因；密钥认证失败是另一个阶段。
- 成功至少要检查实际重新读到的新 UID 和完整第 0 块，不能只看写入 ACK。
- 先恢复 47 块，最后写第 0 块。UID 相同后程序无法区分原卡和副本，普通恢复会拒绝再次写入；只读识别/校验仍可用。
- 此程序没有复制 16 个扇区尾块，不等于完整卡片克隆，更不保证业务系统接受新卡。

历史上曾有新增 Gen1B / CUID 子任务被自动检查拦截，当时只交付了诊断版；之后用户再次明确要求双模式，当前主程序已经实现上述带确认的 Gen1A + CUID 标准认证流程。**不要把历史阻塞误当成当前源码仍没有 CUID 功能。**

## 9. 测试与复现

最新双模式版本已实际编译通过：

```text
Sketch: 981101 bytes / 1310720 bytes (74%)
Global variables: 47624 bytes / 327680 bytes (14%)
```

在项目根目录执行（新电脑先安装 Arduino CLI、开发板包和库）：

```sh
arduino-cli compile --fqbn 'esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=default,PSRAM=disabled' CardBackup
sh tests/run_host_tests.sh
node tests/web_ui_test.mjs
```

主机测试需要 C++11 编译器和 curl；首次会下载两个固定版本的官方接口头文件。网页脚本测试使用 Node.js 18+。主机测试中的 Preferences 接口头文件版本为 3.3.8，实际 ESP32 目标编译使用开发板包 3.3.11；不要混淆两者。

已通过的测试包括：CRC 标准向量、8,384 种单比特损坏、块写入范围、原卡保护、读失败保留旧备份、NVS 重载、单次确认、超时/旧确认码/换卡拒绝、原始第 0 块和源备份变化拒绝、普通与两种第 0 块确认隔离、CUID 准备阶段零次写入、普通卡 NAK、认证会话保持、UID 与 16 字节回读不一致不能报告成功、Web 重复点击和断线恢复。

本地网页预览：

```sh
python3 tests/web_preview.py --card-type cuid
```

打开 [本地预览](http://127.0.0.1:8765)。还可使用 `--card-type gen1a` 或 `--card-type fixed`。它只模拟卡片，绝不会连接真实 RC522；预览 UID 变更不是实机写入证据。

最新网页已在真实浏览器配合模拟后台走通“自动检查 → CUID 确认 → 模拟写入 → 新 UID 显示”，确认过期也已观察到正确处理。较早会话出现过浏览器自动点击超时，后来的这一轮已经正常完成，不是当前功能阻塞。

旧机工具位置，仅作定位参考，不要求新电脑使用相同路径：

```text
Arduino CLI:
/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli

ESP32 core:
/Users/guohai/Library/Arduino15/packages/esp32/hardware/esp32/3.3.11

MFRC522 library:
/Users/guohai/Documents/Arduino/libraries/MFRC522

Temporary host test headers:
/private/tmp/iccard-toolchain

Temporary compiled output:
/private/tmp/iccard-standard-firmware
```

旧机曾运行 `ICCARD_TEST_DEPS=/private/tmp/iccard-toolchain sh tests/run_host_tests.sh` 来复用缓存。新电脑可直接使用默认下载路径，无需保留这个临时目录。

## 10. 淘宝选卡与形态偏好

用户不想用大白卡，更倾向小型钥匙扣、滴胶卡或贴片。助手按要求使用浏览器访问淘宝并查看过商品，没有联系卖家、加购物车或下单。

候选链接（购物信息可能变化）：

- [诺卡智能科技：UID / CUID / UFUID 白卡，多规格](https://item.taobao.com/item.htm?id=613384293252)。当时有 `IC-UID白卡 1张反复擦写使用` 选项；用户觉得形态太大。
- [众新旗舰店：UID 钥匙扣、贴片、滴胶卡](https://detail.tmall.com/item.htm?id=588322210976)。当时看到 `3号KIC（UID）蓝扣10个`、`1#UID（KIC）蓝扣-10个装`、`UID手机贴35mm特价随机10张` 和多种 `UID滴胶` 选项。

**这些页面没有明确证实 Gen1A 指令兼容性，不能把商品俗称当成检测结果。** 曾建议让卖家确认 13.56MHz、M1/S50 1K、4字节UID、第0块可反复写，以及实际协议代际。没有收到卖家的确认。

早先“1号钥匙扣一定更小”等推断没有实际尺寸依据，后续应看尺寸图或卖家说明。商品名称中的 UID、CUID、FUID、UFUID 常有混用，不能仅凭外形或名称保证与本程序兼容。

## 11. 其他讨论：厂商识别与手机 NFC

### 能否从第 0 块判断卡片厂商

- 用户这些卡都是 4 字节 UID。NXP AN10927 明确说明这种 UID 没有通用厂家代码。
- 7/10 字节 UID 的首字节有注册厂家编码概念，但兼容卡、可改 UID 卡可以伪装，不能单靠可写字段认证芯片真伪。
- `9EBA0902` 卡的尾 8 字节为 `62 63 64 65 66 67 68 69`，ASCII 为 `bcdefghi`；这是一个可辨认的数据模式，**不能据此确定生产商或是否可改 UID**。
- `SAK 08` 只表明卡片表现出相应兼容类型；第 0 块中看似 SAK/ATQA 的字节也不是所有厂商都使用同一布局。

### 手机 NFC 和手机交通卡号

用户曾询问 RC522 能否读手机 NFC，以及读取手机公交卡号。当前项目仅实现 M1 数据读写，没有实现手机交通卡所需的业务协议。

需要区分：

- 射频层 UID 与钱包中的交通卡业务卡号不是同一个概念。
- MFRC522 常用库/本项目没有现成的手机交通卡 ISO-DEP/APDU 业务读取流程。
- 手机交通卡使用安全元件，并不意味着任何公开卡号都绝对无法被合适读卡器读取；具体取决于卡种、协议和权限。此前“RC522 绝对不能读任何手机信息”等说法应理解为当前库/程序能力边界，不要扩展成所有硬件/协议的绝对结论。
- 查询自己交通卡号，最直接的途径通常是钱包的卡片详情或发行方 App；已查到华为官方路径为“钱包 → 交通出行 → 对应卡 → 卡片信息”。iPhone 可从钱包中的卡片详细信息查看，是否显示完整卡号视发行方而定。
- 用户尚未提供手机品牌/型号及交通卡名称，也没有提出或实现对应硬件/协议改造。

## 12. 换机后继续工作建议

1. 确认项目拷贝完整，尤其是 Git 当前未跟踪的网页源码与测试文件。
2. 安装已验证的开发板包/库，按原分区和 USB CDC 设置先编译。
3. 连接原 ESP32，先通过 `status` 核对板上源备份是否仍为 `AB0A1EBB`、CRC `CE4107FF`。不要为了识别新卡而覆盖备份。
4. 若要验证最新写法，先确认板子已烧录双模式版：`help` 应列出 `prepare-auto`、`prepare-cuid` 和 `WRITECUID`。
5. 移走原卡，只放目标卡，执行 `prepare-auto` 或在网页选择自动模式。准备成功只说明可以进入确认，不是已写入。
6. 只有用户确认本次实际覆盖后，再按程序提示执行写入；保留从检查开始到重新识别/完整回读的日志。
7. 若仍被拒绝，先区分认证失败、目标变化、访问位不符、NAK、超时、新 UID 不符和回读不符；不要靠修改成功提示或跳过验证来“修复”。

尚待用户反馈的是实体目标卡的最新 CUID / 自动模式结果；尚待确认的是目标卡确切型号与是否支持修改厂家块。没有证据可以保证套装附卡最终能改 UID。

### 给新会话的简短开场

可把下面这段文字和本文件交给新的开发助手：

> 请先阅读项目根目录的 CHAT_CONTEXT.md、README.md 和当前源码。项目是 ESP32-S3 + RC522 的 Arduino 程序，已实现 Wi-Fi 网页、普通 M1 用户数据备份/恢复、Gen1A 和 CUID 标准认证第 0 块写入、自动选择待尝试写法。普通 47 块恢复已实机成功；最新 UID/CUID 模式只完成编译及模拟验证，尚待实体卡结果。原卡 UID 为 AB0A1EBB，备份 CRC 为 CE4107FF，47 个用户数据块全零。请保留板上 NVS，核对完整工程文件后再继续；不要沿用早期“CUID 尚未实现”的说法，也不要把识别失败当成确定的卡型鉴定。

## 13. 已使用的主要资料

- [MFRC522 Arduino 库](https://github.com/miguelbalboa/rfid)
- [MFRC522 1.4.12 源码](https://github.com/miguelbalboa/rfid/blob/1.4.12/src/MFRC522.cpp)
- [NXP MFRC522 数据手册](https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf)
- [NXP MIFARE Classic EV1 1K 数据手册](https://www.nxp.com/docs/en/data-sheet/MF1S50YYX_V1.pdf)
- [NXP UID 处理说明 AN10927](https://www.nxp.com/docs/en/application-note/AN10927.pdf)
- [Proxmark3 可改 UID 卡型说明](https://github.com/RfidResearchGroup/proxmark3/blob/master/doc/magic_cards_notes.md)
- [Arduino-ESP32 安装说明](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- [ESP32 USB CDC 烧录说明](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/cdc_dfu_flash.html)
- [华为钱包查看交通卡卡号](https://consumer.huawei.com/cn/support/content/zh-cn15983268/)
- [Apple 钱包中国大陆交通卡说明](https://support.apple.com/zh-cn/108373)
