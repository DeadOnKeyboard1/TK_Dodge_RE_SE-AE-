# TK Dodge RE — Universal SE/AE/VR (`0.4.10.2`)

[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-737373?style=for-the-badge)](https://learn.microsoft.com/en-us/windows/)
[![Skyrim: 1.7.104.0](https://img.shields.io/badge/Skyrim-1.7.104.0-007ec6?style=for-the-badge)](#target-runtimes--compatibility-status)
[![Edition: SE / AE / VR](https://img.shields.io/badge/Edition-SE%20%2F%20AE%20%2F%20VR-00599C?style=for-the-badge)](#target-runtimes--compatibility-status)
[![SKSE: 2.3.1](https://img.shields.io/badge/SKSE-2.3.1-e87039?style=for-the-badge)](https://skse.silverlock.org/)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-78b900?style=for-the-badge)](LICENSE)

SKSE64 C++23 port of **TK Dodge RE** (originally by **max-su-2019**) updated via **CommonLibSSE-NG v12.1.0** for **Skyrim AE 1.7.104.0** while preserving SE/AE/VR support and optional in-game **SKSE Menu Framework** settings & keybinding configuration (`EnableSneakKeyDodge` supported).

## Target Runtimes & Compatibility Status

* **Mandatory Primary Target:** **Skyrim AE 1.7.104.0** (SKSE64 2.3.1, Address Library Format 5)
* **Build Configuration:** `ENABLE_SKYRIM_SE=1`, `ENABLE_SKYRIM_AE=1`, `ENABLE_SKYRIM_VR=1` (`StructsPost629` + `AddressLibraryV5`)
* **Original Mod Assets Still Required:** **Yes** — this package provides the DLL update (`SKSE/Plugins/TK_Dodge_RE.dll`) and requires the existing **TK Dodge RE** behavior/animation files and `TK Dodge RE.ini`.

| Skyrim Runtime | Matching SKSE | Status |
| :--- | :--- | :--- |
| **1.7.104.0** (Primary AE) | **2.3.1** | Statically verified (`Sprint`/`Sneak` slot 6, `PlayerAnimationEvent` slot 1, RTTI `ActorState`+0xC0 / `ActorValueOwner`+0xB8); **User confirmed** |
| **1.6.1170.0** (Steam AE) | **2.2.8** | Preserved AE branch (`slot 4`, IDs resolve); **Requires in-game test** |
| **1.6.1179.0** (GOG AE) | **2.2.6** | Preserved AE branch (`IDs` resolve); **Requires in-game test** |
| **1.5.97.0** (SE) | **2.0.20** | Preserved SE branch (`slot 4`, IDs resolve); **Requires in-game test** |
| **Skyrim VR** | Matching SKSEVR | Preserved VR preset (`ENABLE_SKYRIM_VR=1`); **Requires in-game test** |

## Compatibility & Key Features

* Ported to **CommonLibSSE-NG v12.1.0** (`commit 8ffc2b74af015f837de53cc02b0cb470a15678e4`).
* Uses `AsActorState()` / `AsActorValueOwner()` accessors cleanly across pre-629 and post-629 `Actor` layouts, and dynamically selects vtable slot `6` (`1.7.99+` flat) vs slot `4` (`< 1.7.99` / VR) for `SprintHandler` and `SneakHandler`.
* Implements configurable **Sneak-Key Tap Dodge** (`EnableSneakKeyDodge`, `SneakingPressDuration`), tap-vs-hold sprint gesture disambiguation, and optional **SKSE Menu Framework** UI (detected dynamically at runtime via `GetModuleHandleW` / `GetProcAddress`, no hard DLL import).

## End-User Installation

Install `SKSE/Plugins/TK_Dodge_RE.dll` as an update over your existing **TK Dodge RE** installation (keeping your existing behaviors, animations, and `TK Dodge RE.ini`). Requires matching **SKSE64** and **Address Library for SKSE Plugins** (**SKSE Menu Framework** is optional).

*(Note on debug symbols: the confirmed release archive distributes `TK_Dodge_RE.dll` only without the developer `.pdb` so no local build paths are shipped.)*

## Reproducible Build Instructions

### Build Requirements
* Windows x64, MSVC C++23 toolset (Visual Studio 2022/2026), CMake `>= 3.25`, Ninja
* **CommonLibSSE-NG v12.1.0** (`https://github.com/alandtse/CommonLibSSE-NG`, commit `8ffc2b74af015f837de53cc02b0cb470a15678e4`)
* **SKSE Menu Framework 3 API** header (`https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API`, commit `1dcb70179076aae4ab626f43c5baab2735ca5877`)
* `vcpkg` / CommonLib dependencies (`simpleini`, `spdlog`, `fmt`, `rapidcsv`, `directxtk`, `directxmath`)

```powershell
cmake -S . -B build/release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_TESTING=ON `
  -DCommonLibSSEPath="path/to/CommonLibSSE-NG-v12.1.0" `
  -DSMF_API_INCLUDE="path/to/SKSE-Menu-Framework-3-API" `
  -DCMAKE_PREFIX_PATH="path/to/deps;path/to/vcpkg-installed/x64-windows-static-md"

cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure
```

## Attribution & License

* Original **TK Dodge RE** work: **Copyright (c) 2023 max-su-2019** (MIT License; notice preserved in [NOTICE](NOTICE)).
* This CommonLibSSE-NG port is licensed under **GNU GPL v3.0 or later (`GPL-3.0-or-later`)** (see [LICENSE](LICENSE)) with **Modding Exception** and **GPL-3.0 Linking Exception** (see [EXCEPTIONS.md](EXCEPTIONS.md)).
