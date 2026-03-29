# Building and Testing KVM-Drivers

## Prerequisites

- Windows 10 or Windows 11 (64-bit)
- Visual Studio 2022 Community or higher
- Windows Driver Kit (WDK) for Windows 11
- Windows SDK

## Building

### Method 1: Visual Studio

1. Open `KVM-Drivers.sln` in Visual Studio 2022
2. Select `Release` or `Debug` configuration
3. Select `x64` platform
4. Build Solution (Ctrl+Shift+B)

### Method 2: Command Line

```powershell
# In PowerShell or Command Prompt
cd P:\KVM-Drivers
.\scripts\build.bat [Release|Debug]
```

## Installing Drivers

**IMPORTANT: Requires Administrator privileges**

### Install (Release)
```powershell
.\scripts\install.bat
```

### Install (Debug)
```powershell
.\scripts\install.bat Debug
```

### Uninstall
```powershell
.\scripts\uninstall.bat
```

## Testing

### Keyboard Driver Test

```powershell
cd build\Release
test_keyboard.exe
```

Options:
- 1: Inject 'A' key
- 2: Inject 'Enter' key
- 3: Inject Ctrl+C combo
- 4: Reset keyboard
- 5: Exit

### Mouse Driver Test

```powershell
cd build\Release
test_mouse.exe
```

Options:
- 1: Move right 100px
- 2: Move left 100px
- 3: Left click
- 4: Scroll up
- 5: Move to screen center
- 6: Exit

### VNC Server Test

Connect with any VNC client to `localhost:5900`

## Running the System Tray Application

```powershell
cd build\Release
KVMTray.exe
```

Or run from Visual Studio: Set `tray` project as Startup and press F5.

## Troubleshooting

### Driver fails to install
- Ensure Secure Boot is disabled (for test-signed drivers)
- Check that you're running as Administrator
- Verify driver signature: `signtool verify /v build\Release\vhidkb.sys`

### Driver not found errors
- Verify drivers are installed: `pnputil /enum-drivers`
- Check Device Manager for "Virtual HID Keyboard", "Virtual HID Mouse"
- Try restarting after driver installation

### Build errors
- Verify WDK is installed and environment variable `WDK_CONTENTROOT` is set
- Check that Visual Studio has C++ and Windows Driver development workloads

## Development

### Adding New IOCTL Commands

1. Define IOCTL code in `src/drivers/<driver>/<driver>_ioctl.h`
2. Add handler in `src/drivers/<driver>/<driver>.c`
3. Update user-mode interface in `src/usermode/core/driver_interface.cpp`

### Protocol Extensions

WebSocket JSON-RPC protocol handlers go in `src/usermode/remote/native/`
VNC protocol extensions go in `src/usermode/remote/vnc/`
