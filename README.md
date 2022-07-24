# Quake III VR Edition

<a href="https://discord.gg/9j6hVD3xAa"><img src="https://img.shields.io/discord/950865674505437214?color=7289da&logo=discord&logoColor=white" alt="Discord server" /></a>
 
This is an early version, only the 3Dof version has been tested in multiplayer, and since then many modifications have been made.

This is a VR, Vulkan only atm, version of Quake III arena, for PCVR, VRAPI (quest / GO / Gear), and OpenXR device. (It also works 2D on most system, android, raspery...)
Based on Cyrax's work Quake3e for the client part, and Cyrax's baseq3a for the QVM (the gamecode).

* Quake3e is a modern Quake III Arena engine aimed to be fast, secure and compatible with all existing Q3A mods. It is based on last non-SDL source dump of ioquake3 with latest upstream fixes applied.
This fast Vulkan renderer allow Quake III arena VR Edition to run 72fps on Oculus GO, and 120fps on Quest2.

* baseq3a is Cyrax's Quake3 gamecode, with unlagged weapons, bugs fixes, faster server scanning, etc.

Quake III arena VR Edition also contains QuakeLive items timer, kindly provided by [Megamind](https://github.com/briancullinan "Megamind"), and the [Open Arena](https://github.com/OpenArena/ "Open Arena") grappling hook. Both can be enable with server commands.

Quake III VR Edition also contains a virtual menu reinforcing the immersion, and a virtual keyboard allows Quake3 to have easy keyboard access on any system. Both are rendered with game calls.

There is also a specially designed [HUD parser](https://github.com/GUNNM-VR/baseq3a-NeoHud "HUD parser").

Here you can see a video of the OculusGo version: https://www.youtube.com/watch?v=-jgpVbIBq7Y&t=2s

First aiming the OculusGo, Quake III arena VR Edition contains a solid 3Dof mode.
The 6Dof is not finished at the moment, better not to activate it. I will resume work on it as soon as possible.

## A native and a non-native version
The non-native version works like the standard version of Quake3.
Position and angles of the VR headset and controllers are sent to the server and therefore to other players.
Other players can then accurately reconstruct angles and positions for weapon and head.
This is the version to use for competitive play, the downside is that all players must be running the same QVM version.


The native version allows playing on any standard Quake 3 servers, as long as they allow mods. (pure server option is "OFF")
Weapon angles and position is reconstruct in order to to best match player aiming.
It's not suitable for competitive play.


## Know bugs
Keyboard is not finished : you get stuck if you clic on "colors" key.
There is a problem with Quest 1, it may be a VRAPI version issue, as the OculusGo version works on Quest I, and it's the only difference.
Non-native 6Dof need positionnal tracking to be set. Some work reconciliating native/non-native version need to be done before.


## Compiling the QVM
Choosing between "native" and "non-native" is done with preprocessor commands, they are defined in "code\game\q_shared.h"

You can have native and non-native qvm in baseq3 folder. The one chosen depends of the client compilation settings.
Compiling the non-native QVM is mandatory as it also creates the pak8aVR.pk3 with the necessary resources.
It's easily done with "baseq3a_Quake3eVR\build\win32-qvm\compile.bat".
Place the pak8aVR.pk3 file obtained in your baseq3 directory.

The native PCVR OpenXR compilation is done with Visual studio solution : build\win32-msvc\baseq3e.sln

## Compiling the OpenXR Client
We also need preprocessors defined, they are in the visual studio compilation definition, as -Dflags , just choose the one you want :
- Debug_QVM_OpenXR
- Debug_Native_OpenXR

## Compiling the Quest app
Preprocessor command are also used as -Dflags, they are in Application.mk
You must have the client folder next to the Android Studio Solution root folder, in order to compile the Android client.
Here is the structure you need:
	
	* yourQuake3VRFolder/
	
		* Quake-III-Arena-VR-Edition/      ***
		
		* baseq3a-VR-Edition/
		
		* Quest-Quake-III-VR-Edition/

*** Update 28/06/2022 :

You need to rename client folder "Quake-III-Arena-VR-Edition" to "Quake3e-master"

TODO : change this folder in Android Studio Solution.

		
## Compiling the Go/Gear app
Same as "Compiling the Quest app" but with the Android Studio solution : GoGear-Quake-III-VR-Edition/
As VRAPI no longer supports oculusGo, an older VRAPI, the v1.5, is included.


## Running the game
Because of different gamma setting between OpenXr and VRAPI, two autoexec.cfg files are provided in Quake-III-Arena-VR-Edition/autoexec/
Copy the one you need in your baseq3 folder, next to pak8aVR.pk3, and other Quake3 pk3 files.


## Links

* https://github.com/ec-/Quake3e
* https://github.com/ec-/baseq3a
