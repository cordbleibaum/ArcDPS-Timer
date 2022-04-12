# ArcDPS Timer Addon

This addon aims to make speedrunning fractals more convenient by having an easy to use timer.

## Contact
For any errors, feature requests and questions, simply open a new issue here. 

## Installation
Make sure arcdps is installed. If arcdps is not installed, this plugin is simply not loaded and does nothing.  
Download the latest version from [github releases](https://github.com/cordbleibaum/ArcDPS-Timer/releases).  
Then put the .dll file into the same folder as arcdps (normally `<Guild Wars 2>/bin64` or `<Guild Wars 2>/addons/arcdps` if using mod loader).  
To disable, just remove the .dll or move it to a different folder.


## Translations
Translations can be added by creating a file in `<Guild Wars 2>/addons/arcdps` called `timer_translation.json`.
To create your own translation you can take the example file in [translation](/translations).

## LICENSE

This project is licensed with the MIT License.

### Dear ImGui
[Dear ImGui](https://github.com/ocornut/imgui) is also licensed with the MIT License and included as git submodule to this project.

### json
[json](https://github.com/nlohmann/json) is a json library created by nlohmann and licensed with the MIT License. It is included into this project with a single file [json.hpp](/json.hpp).

### cpr
[cpr](https://github.com/whoshuu/cpr) is a http library, that is licensed with the MIT License. It is included with vcpkg and itself has dependencies.

## boost
[boost](https://www.boost.org/) is a general collection of C++ libraries, that is licensed with the Boost Software License. It is included with vcpkg
