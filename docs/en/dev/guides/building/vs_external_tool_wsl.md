# Visual Studio External Tool Automation (Windows + WSL)

This page documents the Visual Studio External Tool workflow used to run Cataclysm: BN CMake build
automation from a prompt-driven `cmd` interface.

## Overview

- The workflow is launched from Visual Studio's **External Tools** menu.
- The tool opens a `cmd` window and guides the user with prompts, so manual shell commands are not
  required.
- The same flow can drive Windows builds and WSL-based Linux builds.

## Add the external tool in Visual Studio

Open **Tools -> External Tools...** and configure the build tool entry.

![Visual Studio External Tools menu](https://github.com/user-attachments/assets/a7b5d4b8-2cd3-41be-98ae-e75997619a2c)

![External tool configuration example](https://github.com/user-attachments/assets/197c59df-ac2e-4e2a-99f4-8a5dab860367)

## Runtime interface

When started, the automation opens a `cmd` session with a menu-style prompt flow:

![Prompt-driven cmd interface](https://github.com/user-attachments/assets/934ce9eb-37db-482d-b100-afd7fa215ed8)

Although this runs in `cmd`, users only answer prompts and select options; no manual command
authoring is needed.

Short demo video:

<video controls src="https://github.com/user-attachments/assets/54968297-3b4c-462f-9bae-bd5c1e190b75"></video>

## Build completion

The workflow reports completion in the same command window:

![Build completion output](https://github.com/user-attachments/assets/b43f3130-77c0-4beb-91a0-3b98cb5915f8)

After completion, the built game can be launched normally:

![Cataclysm: BN running after build](https://github.com/user-attachments/assets/3eaf7f95-5653-4c7d-acdd-7977c81ca0ee)
