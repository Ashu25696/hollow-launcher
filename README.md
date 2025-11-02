<H1 align="center">Hollow Launcher</H1>

<a href="https://github.com/Ashu25696/hollow-launcher/blob/main/README_RU.md">Readme на русском</a>

<img src="https://github.com/Ashu25696/hollow-launcher/blob/main/app_pojavlauncher/src/main/assets/pojavlauncher.png" align="left" width="150" height="150" alt="Hollow Launcher logo">

[![Android CI](https://github.com/Ashu25696/hollow-launcher/workflows/Android%20CI/badge.svg)](https://github.com/Ashu25696/hollow-launcher/actions)
[![GitHub commit activity](https://img.shields.io/github/commit-activity/m/Ashu25696/hollow-launcher)](https://github.com/Ashu25696/hollow-launcher/actions)
[![Discord](https://img.shields.io/discord/1365346109131722753.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/RBDEjJX8ZM)

* Hollow Launcher is a **fork of [MojoLauncher](https://github.com/MojoLauncher/MojoLauncher)**, itself based on [PojavLauncher](https://github.com/PojavLauncherTeam/PojavLauncher).  
* It allows you to play Minecraft: Java Edition on your Android device, with support for mods via Forge and Fabric.

## Navigation
- [Introduction](#introduction)  
- [Getting Hollow Launcher](#getting-hollow-launcher)
- [Building](#building) 
- [Current roadmap](#current-roadmap) 
- [License](#license) 
- [Contributing](#contributing) 
- [Credits & Third party components](#credits--third-party-components)

## Introduction
* Hollow Launcher is a Minecraft: Java Edition launcher for Android based on MojoLauncher.  
* This fork includes [your modifications / features] and keeps support for almost all Minecraft versions.  

## Getting Hollow Launcher
You can get Hollow Launcher via:

1. Prebuilt app from [GitHub Releases](https://github.com/Ashu25696/hollow-launcher/releases).  
2. Google Play (if published).  
3. Build from source.

## Building
* Build the launcher (it will automatically download required components):

```bash
./gradlew :app_pojavlauncher:assembleDebug
