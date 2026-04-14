# Hellspawn
Data editor for [forgottenserver](https://github.com/otland/forgottenserver/) and [otclient](https://github.com/edubart/otclient/).

Currently only .dat version `10.98` is supported.

Made with the assistance of `Claude Opus 4.6`.

# Screenshots
<img width="1340" height="828" alt="image" src="https://github.com/user-attachments/assets/101d19a2-3fbb-4333-9d64-788dfabefa5f" />
<img width="1340" height="828" alt="image" src="https://github.com/user-attachments/assets/76ebdd26-48ab-465e-9e2b-e48dac776beb" />
<img width="1340" height="828" alt="image" src="https://github.com/user-attachments/assets/47e31d6a-f7a2-4da2-a958-c4f3eddda5ae" />

# Build Instructions (Windows)
* Requirements: Git, CMake, MSBuild/VS (with C++20 support), vcpkg.
* Steps
	* `git clone git@github.com:diath/hellspawn.git`
	* `cd hellspawn`
	* `build.bat`

# Build Instructions (Linux)
* Requirements: Git, CMake, GCC or Clang (with C++20 support), vcpkg (or required system libraries: Qt6, fmtlib, pugixml).
* Steps:
	* `git clone git@github.com:diath/hellspawn.git`
	* `cd hellspawn`
	* `chmod +x build.sh`
	* `./build.sh`

# Pre-built Binaries
Pre-built binaries are available through [GitHub Releases](https://github.com/diath/hellspawn/releases).

# TODO
Non-exhaustive list of nice-to-have features that are currently missing in the editor and other things:
* Support for importing T12+ assets.
* Support for other _common_ versions of the .dat file format (7.1, 7.4, 7.6, 8.6).
* Support for older .spr file format (16-bit count header).
* Support for visual grid-based sprite assembler for large objects.

# License
This project is licensed under the [3-Clause BSD License](LICENSE.md).

# Icon
The application [icon](https://game-icons.net/1x1/lorc/spark-spirit.html) was created by [Lorc](https://lorcblog.blogspot.com/) under [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/).
