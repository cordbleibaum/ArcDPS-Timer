# ArcDPS Timer Addon

This addon aims to make speedrunning in GW2 more convenient by having an easy to use timer. It also has a segment function for measuring splits.

## Feature overview

* Group sync for timer (in instances via server IP, otherwise via group member list)
* Auto start
* Auto stop (experimental)
* Timing segments
* Automatic segments
* Fully keybindable
* Translation support
* Logging

Overall status: beta version, some first testing done, but likely still many bugs

## Contact
For any errors, feature requests and questions, simply open a new issue here. 

## Installation
Make sure arcdps is installed. If arcdps is not installed, this plugin is simply not loaded and does nothing.  
Download the latest version from [github releases](https://github.com/cordbleibaum/ArcDPS-Timer/releases).  
Then put the .dll file into the same folder as arcdps (normally `<Guild Wars 2>/bin64` or `<Guild Wars 2>/addons/arcdps` if using mod loader).  
To disable, just remove the .dll or move it to a different folder.

## Troubleshooting
If the DLL is not loaded an d the options do not show up, make sure, that you have installed the latest C++ 2015-2022 Redistributable.
You can download the latest Redistributable from [Microsofts download page](https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170). 

## Translations
Translations can be added by creating a file in `<Guild Wars 2>/addons/arcdps` called `timer_translation.json`.
To create your own translation you can take the example file in [translation](/translations).

## LICENSE

This project is licensed with the MIT License.

### Dear ImGui
[Dear ImGui](https://github.com/ocornut/imgui) is also licensed with the MIT License and included as git submodule to this project

### json
[json](https://github.com/nlohmann/json) is a json library created by nlohmann and licensed with the MIT License. It is included with vcpkg

### cpr
[cpr](https://github.com/libcpr/cpr) is a http library, that is licensed with the MIT License. It is included with vcpkg and itself has dependencies

### boost
[boost](https://www.boost.org/) is a general collection of C++ libraries, that is licensed with the Boost Software License. It is included with vcpkg

### hash-library
[hash-library](https://github.com/stbrumme/hash-library) is a hashing library, that is licensed with the Zlib license. It is included as git submodule

### arcdps-extension
[arcdps-extension](https://github.com/knoxfighter/arcdps-extension) is a arcdps extension library under MIT license. It is included as git submodule

### Eigen
[Eigen](https://eigen.tuxfamily.org/index.php?title=Main_Page) is a linear algrebra library under MPL2 license. It is included via vcpkg
