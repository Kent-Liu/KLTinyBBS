# KLTinyBBS

> A lightweight BBS module that runs directly on a Meshtastic node — no Raspberry Pi, no connected computer, even solar-powered nodes can run it.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

[繁體中文](README.md)

---

## Background

Open-source BBS tools that rely on a companion Python host have been around for a while, but attaching an external computer or Raspberry Pi is far too fragile for emergency or off-grid scenarios. [LoBBS](https://github.com/MeshEnvy/lobbs) was a great inspiration — hosting the BBS on the node itself is the right approach.

After studying LoBBS carefully, it became clear that its storage design is not well-suited to the nRF52 series: frequent writes can cause excessive Flash wear and shorten hardware lifespan. Building on the conceptual inspiration of LoBBS's account, mail, and news features, this module was redesigned from scratch (**not a fork of LoBBS**) with a storage mechanism better suited to embedded environments, allowing both nRF52 and ESP32 to run reliably over the long term.

---

## Features

- **Cross-platform**: Supports nRF52 and ESP32, including solar-powered nodes
- **Flash-friendly**: Uses an A/B wear-leveling write scheme to minimize wear on nRF52's limited Flash
- **Full UTF-8 support**: Chinese and multilingual messages display and truncate correctly
- **Private mode**: Password-lock the BBS so only family or a specific group can use it (perfect as a family-only communication station!)
- **New mail indicator**: A ✉️ prompt appears on login when there is unread mail
- **Message length warning**: When a post or private message exceeds the length limit, the overage is reported — messages won't silently disappear
- **Custom welcome message**: Admins can set a login greeting, ideal for broadcasting important information in emergencies

> Due to resource constraints on the nRF52 platform, the account limit (max 8) and message storage (2 KB) are smaller than on ESP32 (32 accounts / 16 KB). Plan your deployment accordingly.

---

## Tested Environments

| Firmware Version | Hardware |
|------------------|----------|
| Meshtastic 2.7.15 | Heltec V4、Heltec V3、Heltec WSL V3、Seeed T1000-E、Seeed XIAO nRF52、Gat562 |

If you test on other firmware versions or hardware and it works, please share your results! Open an Issue or PR.

---

## Installation and Compilation

This module must be integrated into the official Meshtastic firmware source before building. See the detailed steps in:

**[Compile Integration Guide](docs/COMPILE_GUIDE_en.md)**

---

## Documentation

- **[User Guide](docs/USER_GUIDE_en.md)**: Login, sending and receiving private mail, news posts, user list, and other general operations
- **[Admin Guide](docs/ADMIN_GUIDE_en.md)**: Admin mode, Private mode, reset, and other management functions

---

## ⚠️ After First Installation

**Change the default admin password immediately!**

The default password is `12345678`. After flashing, enter admin mode and change it right away:

```
/admin 12345678
/admin pass yournewpassword
/admin bye
```

Leaving the default password is equivalent to leaving the door wide open — you are responsible for the consequences 😅

---

## Contributing

Found a bug, have a suggestion, or want to share an idea? Open an [Issue](../../issues) to discuss; if you have improved code, submit a [Pull Request](../../pulls). Whether it's reporting tested hardware, fixing a typo in the docs, or proposing a new feature — all contributions are greatly appreciated!

---

## Disclaimer

This module is provided AS IS. Users assume all risks associated with installation and use. The author is not responsible for any data loss, hardware or software damage, or any other direct or indirect losses.

---

## License

[MIT License](LICENSE)
