# Nostalgia Dumper

Nostalgia Dumper is an industry-leading, high-performance offset discovery and memory forensics tool designed for game developers, security researchers, and reverse engineers.

Built from the ground up for speed and reliability, it supports a wide range of game engines and enables extraction of global variables, pointers, and named exports with high accuracy.

---

## Overview

Nostalgia Dumper provides a unified interface for:

* Unreal Engine structure analysis
* Unity IL2CPP and Mono inspection
* Global variable discovery across engines
* Export and module analysis

---

## Core Capabilities

### Multi-Threaded Scanning

Efficiently distributes pattern scanning across all CPU cores for high-speed memory analysis.

### Heuristic Fallback System

When signature scanning fails due to updates or obfuscation, Nostalgia Dumper analyzes surrounding instructions and references to recover valid offsets.

### Memory Forensics Suite

| Feature        | Description                                           |
| -------------- | ----------------------------------------------------- |
| Pattern Finder | Test custom byte signatures in real time              |
| Module Map     | View loaded modules, base addresses, and memory sizes |

---

## Supported Engines

Nostalgia Dumper includes specialized discovery logic for multiple engines:

| Engine                | Discovery Targets                    |
| --------------------- | ------------------------------------ |
| Unreal Engine         | GObjects, GNames, GWorld, GEngine    |
| Unity (IL2CPP)        | GameAssembly.dll, exports, init APIs |
| Unity (Mono)          | Mono.dll, JIT, runtime exports       |
| Source Engine (1 & 2) | GlobalVars, EntityList, Interfaces   |
| Godot Engine          | Core structures, exports             |
| CryEngine             | CrySystem.dll, module exports        |
| GameMaker Studio      | YYObjectArray, global maps           |
| UNIGINE               | World structures, nodes              |
| Universal PE          | Named export harvesting              |

---

## Usage

### 1. Initial Setup

1. Launch the target application
2. Wait until it is fully loaded
3. Run `Nostalgia.exe` (Administrator recommended)

---

### 2. Configuration

* Open the Dashboard
* Select the appropriate engine
* Use **Universal** if unsure

---

### 3. Process Selection

* Locate your process
* Use search if needed
* Click to attach

---

### 4. Execution

```
START DISCOVERY
```

* Initializes scanning engine
* Uses multi-threading
* Displays real-time progress

---

### 5. Results

* View results in **Discovered Data** tab
* Filter by name (e.g., `GWorld`)
* Copy addresses instantly

---

### 6. Exporting

```
GENERATE OFFSET HEADER
```

* Outputs clean C++ header file
* Saves to selected directory

---

## Building from Source

### Requirements

* Visual Studio 2022 (C++ workload)
* CMake 3.22+
* Windows 10/11 SDK

---

### Build Instructions

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

---

### Output

```
build/bin/Release/
```

---

## Project Structure

```
Nostalgia-Dumper/
│
├── src/
├── include/
├── CMakeLists.txt
├── README.md
```

---

## Disclaimer

Nostalgia Dumper is provided for educational purposes, security research, and interoperability testing.

The developers assume no liability for misuse. Ensure compliance with all applicable terms of service and license agreements.
