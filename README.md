
---

# Advanced Screen Recorder

**Created by 10xRashed**

A simple yet powerful screen recording application for Linux/X11 desktops, built with C++ and GTK+. This tool leverages the power of FFmpeg to capture screen activity, providing a user-friendly graphical interface for controlling the recording process.

It allows for both full-screen and regional captures, with options to configure frame rate, quality, and output format.

*(Image placeholder for the application's UI)*

## Features

-   **Intuitive GUI**: A clean and easy-to-use graphical interface built with GTK+ 3.
-   **FFmpeg Backend**: Utilizes the robust and widely-used FFmpeg library for high-performance screen capturing and encoding.
-   **Full Screen & Region Capture**: Easily capture your entire screen or specify an exact rectangular area for recording.
-   **Custom Frame Rate**: Set the desired frames per second (fps) for your recording, from 1 to 60.
-   **Quality Control**: Choose between High, Medium, and Low quality presets to balance file size and visual fidelity. These presets correspond to FFmpeg's `-crf` (Constant Rate Factor) values.
-   **Multiple Formats**: Save recordings in popular video containers like `.mp4`, `.mkv`, `.avi`, and `.webm`.
-   **Real-time Status Updates**: A status bar provides instant feedback on the application's state (e.g., "Ready to record," "Recording started," or error messages).
-   **Safe Shutdown**: The application prompts for confirmation before exiting if a recording is in progress, preventing accidental data loss.
-   **Dependency Check**: Automatically verifies if FFmpeg is installed on startup and provides simple installation instructions if it's missing.

## Prerequisites

Before you can compile and run this application, you need to have the following dependencies installed on your system:

1.  **A C++ Compiler**: A modern C++ compiler like `g++` that supports C++11 or newer.
2.  **GTK+ 3 Development Libraries**: The toolkit used for the graphical interface.
3.  **FFmpeg**: The core backend for screen capturing and encoding.
4.  **X11 Development Libraries**: Required for detecting screen dimensions.
5.  **pkg-config**: A helper tool used to find compiler and linker flags for libraries.

### Installation of Dependencies

You can install the necessary packages using your distribution's package manager.

**On Ubuntu / Debian:**
```bash
sudo apt update
sudo apt install build-essential libgtk-3-dev ffmpeg libx11-dev pkg-config
```

**On Fedora / CentOS / RHEL:**
```bash
sudo dnf install g++ gtk3-devel ffmpeg libX11-devel pkgconfig
```

**On Arch Linux:**
```bash
sudo pacman -S base-devel gtk3 ffmpeg libx11 pkg-config
```

## How to Compile

1.  Save the code to a file named `screen-recorder.cpp`.
2.  Open your terminal and navigate to the directory where you saved the file.
3.  Compile the program using the following command:

    ```bash
    g++ screen-recorder.cpp -o screen-recorder `pkg-config --cflags --libs gtk+-3.0`
    ```
    This command compiles the C++ code (`screen-recorder.cpp`), links it against the GTK+ 3 libraries (using `pkg-config` to supply the correct flags), and creates an executable file named `screen-recorder`.

## How to Run

After successful compilation, you can run the application directly from the terminal:

```bash
./screen-recorder
```

The application window will appear, and the video file will be saved in the same directory from which you launched the executable.

## How It Works

This application acts as a graphical frontend for FFmpeg. Here’s a breakdown of its operation:

1.  **GUI Management (GTK+)**: The user interface is built and managed by the GTK+ library. All buttons, input fields, and labels are GTK+ widgets. User interactions (like button clicks) trigger C++ callback functions.
2.  **Command Construction**: When the "Start Recording" button is clicked, the application reads the values from all input fields (frame rate, filename, quality, region, etc.). It uses this information to dynamically construct a valid `ffmpeg` command-line string.
3.  **Process Execution**: Instead of running FFmpeg directly within its own process, the application uses `popen()` to launch `ffmpeg` as a separate, background child process. It appends `& echo $!` to the command to retrieve the Process ID (PID) of the new `ffmpeg` process.
4.  **State Management**: Once recording starts, the PID is stored globally. The GUI is set to a "disabled" state to prevent changes during recording. The "Stop" button is enabled.
5.  **Process Termination**: When the "Stop Recording" button is clicked, the application sends a `SIGINT` signal to the stored `ffmpeg` PID. This is equivalent to pressing `Ctrl+C` in the terminal and allows FFmpeg to finalize the video file gracefully. If FFmpeg doesn't terminate within a few seconds, a `SIGKILL` signal is sent to force it to stop.

## Limitations and Future Improvements

This application is a functional proof-of-concept but has room for improvement.

#### Current Limitations:
*   **No Audio Recording**: The current implementation only captures video.
*   **X11 Only**: It relies on X11 (`x11grab`) for screen capture and will not work on systems running Wayland by default.
*   **Manual Region Selection**: You must manually enter the coordinates and dimensions for region capture.
*   **No Cursor Capture Option**: The cursor may or may not be captured depending on the system's FFmpeg build. There is no explicit option to control this.
*   **Basic Error Handling**: While it checks for FFmpeg and validates inputs, it may not catch all possible FFmpeg errors.

#### Potential Future Enhancements:
*   **Audio Support**: Add options to select an audio source (e.g., microphone, system audio) using PulseAudio or PipeWire.
*   **Wayland Support**: Integrate with PipeWire and `xdg-desktop-portal` for screen recording on Wayland.
*   **Interactive Region Selection**: Implement a feature to let the user click and drag to select the recording area.
*   **File Save Dialog**: Add a "Save As" button that opens a native file dialog to choose the save location and filename.
*   **System Tray Icon**: Allow the application to be minimized to the system tray during recording.
*   **Advanced Options**: Expose more FFmpeg settings in the UI, such as codec selection, bitrate, and pixel format.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
