# FastGoToFile
Visual Studio extension for quickly jumping to a file in the solution

This is a fork of the [Open Now!](https://marketplace.visualstudio.com/items?itemName=Nem.OpenNow) Visual Studio extension by Ryan Gregg.  Open Now! stopped working with Visual Studio 2019, and isn't very actively maintained, so I decided to make my own copy so I could start making tweaks and improvements.

So far, the changes from Open Now! are:
* Works with Visual Studio 2022 (earlier releases supported Visual Studio 2019)
* Fixed many significant memory leaks
* Made numerous performance fixes

## Extension Features:
* Open files in Visual Studio or view them in Windows Explorer.
* Open files from an optional user selectable directory.
* Force open files in code view (instead of designer view which some file types default to).
* Filter and sort by file name, file path, project name and project path.
* Filter using wildcards.
* Retain settings between uses.
* Fast responsive interface.
* Open complementary file for cycling between header/source, and other related files (this command is hidden by default).
* Open source.

## Special Characters:

There are a number of special characters that can prefix search terms. You can string several special characters together to create more advanced terms. The actual search string starts after the first invalid special character or opening quote. These special characters are:

The ampersand (&) specifies that the search string should be combined with a logical "and". (Note, this is the default implicit behavior - you probably wont even need to type an ampersand).
The vertical bar (|) specifies that the search string should be combined with a logical "or".
The hyphen (-) and exclamation mark (!) specify that the search string should be negated.
The backward slash (\) or forward slash (/) specify that the whole file or project path should be searched, not just the file or project name.
The colon (:) specifies that the project or project path should be searched and not the file name or file path.
The double quote (") can be used to surround the above special characters (or spaces) to effectively "escape" them.
The asterisk (*) can be used as a multi character wildcard.
The question mark (?) can be used as a single character wildcard.

### Special Character Examples:

| Example | Description |
| ------- | ----------- |
| substring.cpp(row[,col])	| Opens the selected file navigating to row/col. Useful for opening based on build break output
| substring.cpp:row[:col]	| Same as above, using the colon-separated form emitted by many compilers and tools (e.g. `foo.cpp:32:10`)
| substring .h	| Find all files containing substring in all .h files.
| substring &#124;.cpp &#124;.h | Find all files containing substring in all .cpp and .h files.
| substring -.h	| Find all files containing substring in all files except .h files.
| substring -.cpp -.h | Find all files containing substring in all files except .cpp and .h files.
| substring \\\\include\\ | Find all files containing substring in any folder called include.
| substring :\"header files" | Find all files containing substring in any filter containing header files.
| :substring | Find all files in any project containing substring.
| substring *.h | Find all files containing substring in all files ending in .h.


## Building from Source

### Prerequisites

* Windows 10 or later (x64).
* Visual Studio 2022 with these workloads:
  * **Desktop development with C++**, including the v143 toolset and a Windows 10/11 SDK.
  * **Visual Studio extension development**, which installs the VS SDK.

The C++ project finds the VS SDK headers and libraries through `VsSDK.props`, which points at `$(VSInstallDir)\VSSDK\VisualStudioIntegration`. If the extension development workload is missing, the build fails on missing headers such as `VSLPackage.h`.

### Solution layout

| Project | Purpose |
| ------- | ------- |
| `GoToFile` | The native VS package (the dialog, search logic and settings). |
| `GoToFileUI` | Satellite resource DLL with the command table (`PackageCommands.vsct`) and images. |
| `GoToFileVSIX` | Packages both DLLs into the `.vsix`, and launches the experimental instance for debugging. |

### Build

Open `GoToFile.sln` in Visual Studio 2022, select the **x64** platform, and build. Or, from a Developer Command Prompt:

```
msbuild GoToFile.sln /m /restore /p:Configuration=Release /p:Platform=x64
```

Warnings are treated as errors (warning level 4), so the build must stay warning-free. CI runs the same command on every pull request.

### Debug

1. Set **GoToFileVSIX** as the startup project. The solution's default startup project is `GoToFile`, which can't be launched on its own.
2. Press F5. This starts `devenv.exe /rootsuffix Exp`, a separate *experimental instance* of Visual Studio with its own settings and extensions, and deploys the freshly built extension into it. Native debugging is enabled, so breakpoints in the C++ code work.
3. In the experimental instance, open any solution and use **Tools > Fast Go To File...** (Ctrl+Shift+Alt+O).

To reset the experimental instance, run **Reset the Visual Studio 2022 Experimental Instance** from the Start menu.

### Package and install

A build writes the extension package (`.vsix`) to `GoToFile\GoToFileVSIX\bin\x64\<Configuration>\`. Double-click it, or run `VSIXInstaller.exe` on it, to install it into your regular Visual Studio. Close Visual Studio first. The package version comes from `GoToFile\GoToFileVSIX\source.extension.vsixmanifest`.

CI also uploads the Release `.vsix` as the `FastGoToFile-vsix` artifact on each run.

## FAQ

#### I've installed FastGoToFile, where can I find it?
By default, FastGoToFile will create a command called "Fast Go To File..." under your "Tools" menu.  The default shortcut is Ctrl+Shift+Alt+O.

#### How can I change the shortcut to FastGoToFile?

To change the shortcut to FastGoToFile:

1. Open Visual Studio.
2. Select "Tools" then "Options...".
3. Select "Environment" -> "Keyboard".
4. Type "FastGoTo" under "Show commands containing:".
5. Select "Tools.FastGoToFile".
6. Press "Remove" to remove the old shortcut (commands can have multiple shortcuts).
7. Type your new shortcut under "Press shortcut keys:".
    a. Recommended shortcut is "Ctrl+Shift+Alt+O", which by default is bound to 'File.OpenFolder'
8. Press "Assign" to add the new shortcut.
9. Press "OK" then "Close".
