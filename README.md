# Open Ephys Data Format

![header-image](Resources/header-image.png)

A Record Engine plugin that saves data in the original Open Ephys data format.

## Installation

This plugin must be added via the Open Ephys GUI Plugin Installer. Before installing a new Record Engine, all Record Nodes must be removed from the signal chain. To access the Plugin Installer, press **ctrl-P** or **⌘P** from inside the GUI. Once the installer is loaded, browse to the "Open Ephys Data Format" plugin and click "Install."

## Usage

A complete description of the Open Ephys Data Format is available [here](https://open-ephys.github.io/gui-docs/User-Manual/Recording-data/Open-Ephys-format.html).

## Building from source

First, follow the instructions on [this page](https://open-ephys.github.io/gui-docs/Developer-Guide/Compiling-the-GUI.html) to build the Open Ephys GUI.

**Important:** This plugin is intended for use with the pre-release core application, version 0.6.0. The GUI should be compiled from the [`development-juce6`](https://github.com/open-ephys/plugin-gui/tree/development-juce6) branch, rather than the `master` branch.

Then, clone this repository into a directory at the same level as the `plugin-GUI`, e.g.:
 
```
Code
├── plugin-GUI
│   ├── Build
│   ├── Source
│   └── ...
├── OEPlugins
│   └── open-ephys-data-format
│       ├── Build
│       ├── Source
│       └── ...
```

### Windows

**Requirements:** [Visual Studio](https://visualstudio.microsoft.com/) and [CMake](https://cmake.org/install/)

From the `Build` directory, enter:

```bash
cmake -G "Visual Studio 17 2022" -A x64 ..
```

Next, launch Visual Studio and open the `OE_PLUGIN_open-ephys-data-format.sln` file that was just created. Select the appropriate configuration (Debug/Release) and build the solution.

Selecting the `INSTALL` project and manually building it will copy the `.dll` and any other required files into the GUI's `plugins` directory. The next time you launch the GUI from Visual Studio, `Open Ephys` should appear as an data format option in the Record Node.


### Linux

**Requirements:** [CMake](https://cmake.org/install/)

From the `Build` directory, enter:

```bash
cmake -G "Unix Makefiles" ..
cd Debug
make -j
make install
```

This will build the plugin and copy the `.so` file into the GUI's `plugins` directory. The next time you launch the compiled version of the GUI, `Open Ephys` should appear as an data format option in the Record Node.


### macOS

**Requirements:** [Xcode](https://developer.apple.com/xcode/) and [CMake](https://cmake.org/install/)

From the `Build` directory, enter:

```bash
cmake -G "Xcode" ..
```

Next, launch Xcode and open the `open-ephys-data-format.xcodeproj` file that now lives in the “Build” directory.

Running the `ALL_BUILD` scheme will compile the plugin; running the `INSTALL` scheme will install the `.bundle` file to `/Users/<username>/Library/Application Support/open-ephys/plugins-api`. `Open Ephys` should now appear as an data format option in the Record Node.


### Attribution

This plugin and the Open Ephys data format specification were collaboratively developed by Josh Siegle, Aarón Cuevas López, and Pavel Kulik.
 
