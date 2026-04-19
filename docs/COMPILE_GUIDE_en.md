# KLTinyBBS Compile Guide

> ⚠️ **Warning 1**
> Compiling this module requires the ability to build the Meshtastic firmware from source and to flash the resulting firmware onto your device. These prerequisites are not covered in this project's documentation — please refer to the official Meshtastic documentation.

> ⚠️ **Warning 2**
> The module has only been compiled and tested against firmware version `2.7.15`. The author does not have time to test every firmware version or hardware combination, though it may still work on others. Use at your own risk; the author is not responsible for any software or hardware issues that may arise.

This document explains how to integrate the KLTinyBBS module into the official Meshtastic firmware source and build it yourself.

---

## Prerequisites

- Prepare a working Meshtastic firmware build environment that compiles successfully.
- Confirm you have write access to the target firmware project directory and can flash your device.
- It is recommended to work on a clean branch or a backup environment to avoid overwriting existing changes.

---

## Integration Steps

1. **Copy the module source into the official directory**
   - Copy the entire `src/modules/KLTinyBBS/` folder from this project into the `src/modules/` directory of the official firmware source.

2. **Add the include in `src/modules/Modules.cpp`**
   - Open `src/modules/Modules.cpp` in the official firmware project.
   - Add the following line in the module include section:
     - `#include "KLTinyBBS/KLTinyBBSModule.h"`

3. **Register KLTinyBBS in the module list**
   - In the `Example: Put your module here` section inside `Modules.cpp`, add:
     - `new KLTinyBBSModule();`

---

## Building and Flashing

- After completing the integration above, build the firmware using your existing Meshtastic build process.
- Once compiled successfully, flash the firmware to your device and test it.
- If the build fails, check the following:
  - Is the `KLTinyBBS/` directory placed at the correct path?
  - Is the include path in `Modules.cpp` correct?
  - Is `new KLTinyBBSModule();` placed inside the correct module registration block?
