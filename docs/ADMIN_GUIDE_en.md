# KLTinyBBS Admin Guide

## ⚠️ Read Before First Use

The default admin password for KLTinyBBS is `12345678`. To prevent unauthorized access to admin mode, **change the admin password immediately after first activation** (`/admin pass newpassword`) before making any other configurations.

---

## Platform Differences and Capacity Limits (nRF52 / ESP32)

KLTinyBBS runs on both nRF52 and ESP32, but storage capacity and available resources differ. Plan your deployment based on the target platform.

| Item | ESP32 | nRF52 | Notes |
|------|-------|-------|-------|
| Max users | 32 | 8 | `KLTINYBBS_MAX_USERS` — limits total registered accounts. |
| Message storage (PM/News shared) | 16384 bytes | 2048 bytes | `KLTINYBBS_PM_FILE_SIZE` — when full, old data must be deleted to free space. |
| Max single message length | 104 bytes | 104 bytes | `KLTINYBBS_PM_RECORD_TEXT_LEN` — applies to both private messages and news posts. |
| Auto-flush interval | 1800 s | 3600 s | `KLTINYBBS_USERSTORE_FLUSH_INTERVAL_SEC` — controls how often data is written to Flash. |
| Private idle re-lock timeout | 900 s | 900 s | `KLTINYBBS_PRIVATE_IDLE_SEC` — auto-locks after 15 minutes of inactivity when unlocked. |
| Max entries returned per list query | 16 | 16 | `KLTINYBBS_LIST_MAX` — maximum entries per single list response. |
| Summary preview lines | 5 | 5 | `KLTINYBBS_LIST_PREVIEW_LINES` — default preview count for `/m` and `/n`. |

---

## Entering and Leaving Admin Mode

- **Enter admin**: Enter `/admin adminpassword`
  - A correct password returns `OK` and activates admin mode.
  - An incorrect password returns an "unknown command" style response to avoid revealing the admin entry point.

- **Leave admin**: Enter `/admin bye`
  - Returns `OK` and exits admin mode.
  - Any pending preference changes are written to storage at this point.

- **View admin commands**: Enter `/h`, `/help`, or `/?` while in admin mode
  - The help output switches to the admin command set.

---

## Admin Functions

The following functions require **admin mode**; most admin commands are treated as unknown commands when not in admin mode.

- **Change admin password**: `/admin pass newpassword`
  - Password rules: 4–8 printable ASCII characters.
  - Change regularly and avoid easily guessable strings.

- **Set login welcome message**: `/welcome message text`
  - This text is appended to the welcome message shown on successful login.
  - There is a length limit (~103 bytes); exceeding it returns a failure notice.

- **Private mode management**:
  - `/private status`: shows whether private mode is currently `locked` or `unlocked`.
  - `/private lock`: enables private mode.
  - `/private unlock`: disables private mode.
  - `/private pass newpassword`: changes the private unlock password (4–8 printable ASCII characters).

- **Delete any news post**: `/delete post N`
  - `N=1` refers to the latest post.
  - Admin can delete any user's post; regular users can only delete their own.

- **Reset the entire BBS**: `/reset adminpassword`
  - Clears all mail, news, user data, preferences, and all login, admin, and private sessions.
  - After a successful reset, both the admin and private passwords revert to the default `12345678`; you will need to re-enter admin to reconfigure.

---

## Admin Limitations and Notes

- Admin and private unlock states are **in-memory sessions** and are lost on reboot or wake from deep sleep.
- Whether private mode is enabled is a persistent setting (PrefStore) and survives reboots.
- Admin and private passwords must be 4–8 printable ASCII characters (no non-ASCII/Unicode characters).
- Response length is limited by packet size; lists and long messages are returned as summaries — query in batches if needed.

---

## Private Mode Overview

Private mode prevents unauthorized nodes from using the BBS. When enabled, a node can perform almost no regular commands until it is unlocked.

- **Behavior when enabled**:
  - The node only accepts `/private password` for unlocking.
  - Other commands receive no response (reduces probing risk).

- **Validity after unlocking**:
  - Entering `/private password` correctly returns `Unlocked.` and enters the usable state.
  - If the node is idle for more than 15 minutes, it automatically re-locks.
  - Each valid operation during the unlocked period resets the 15-minute timer.

- **Relationship with login state**:
  - When private re-locks due to inactivity, existing account bindings are not forcibly logged out.
  - After unlocking again on the same node, the original login state is usually still active.

---

## Private Mode Setup Tutorial (Recommended Flow)

The following workflow is suitable for real deployments (e.g., fixed nodes, shared family devices):

1. Enter admin mode with `/admin adminpassword`.
2. Run `/private pass newpassword` to update the private password first.
3. Run `/private lock` to enable private mode.
4. Test from a regular user node: send a normal command (should receive no response), then send `/private newpassword` (should return `Unlocked.`).
5. Once verified, the admin can exit admin mode with `/admin bye`.
6. If you want to further conceal the device's existence, you can also configure it as `CLIENT_HIDDEN` in the device settings.

To temporarily open the BBS without locking, the admin can enter admin mode and run `/private unlock`; re-apply `/private lock` when done.
