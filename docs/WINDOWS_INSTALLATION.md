# Windows Installation Guide

This guide explains how to install the `php-firebird` extension on Windows using the pre-compiled DLLs available from our GitHub releases.

## 1. Download the Correct DLL

We provide pre-compiled DLLs for various combinations of PHP versions, architectures, and Firebird client versions. You must select the DLL that matches your environment exactly.

Go to the [Releases Page](https://github.com/satwareAG/php-firebird/releases) and find the latest release.

### Naming Convention

The DLLs follow this naming format:
`php_firebird-{ext-ver}-php{php-ver}-{ts}-{arch}-fb{fb-ver}.dll`

| Part | Meaning | Example |
|------|---------|---------|
| `{ext-ver}` | Extension version | `7.0.0` |
| `{php-ver}` | PHP version | `8.4` |
| `{ts}` | Thread Safety | `ts` (Thread Safe) or `nts` (Non-Thread Safe) |
| `{arch}` | Architecture | `x64` (64-bit) |
| `{fb-ver}` | Firebird Client Version | `5.0` |

### How to Check Your PHP Environment

Run the following command to determine your PHP configuration:

```powershell
php -i | Select-String "PHP Version", "Architecture", "Thread Safety"
```

Output example:
```text
PHP Version => 8.4.1
Architecture => x64
Thread Safety => enabled
```

In this example, you need:
- PHP Version: **8.4**
- Architecture: **x64**
- Thread Safety: **ts** (enabled = Thread Safe)

If you are using Firebird 5.0, you would download:
`php_firebird-7.0.0-php8.4-ts-x64-fb5.0.dll`

## 2. Install Firebird Client Library

The extension requires the Firebird client library (`fbclient.dll`) to be available on your system.

1. Download the Firebird Windows ZIP kit matching your DLL's Firebird version (3.0, 4.0, or 5.0) and architecture (x64).
   - [Firebird Downloads](https://firebirdsql.org/en/downloads/)
2. Extract `fbclient.dll` from the ZIP.
3. Place `fbclient.dll` in a directory that is in your system `PATH`, or in the same directory as `php.exe`.

**Note:** If you already have Firebird Server installed on the same machine, the client library might already be in your PATH.

## 3. Install the Extension

1. Copy the downloaded `php_firebird.dll` to your PHP extensions directory (usually `ext/`).
   - You can rename it to `php_firebird.dll` for simplicity, or keep the full name.

2. Edit your `php.ini` file:
   ```ini
   extension=php_firebird.dll
   ```
   (Use the actual filename if you didn't rename it)

3. Verify the installation:
   ```powershell
   php -m | Select-String firebird
   ```
   If successful, it will output `firebird`.

## 4. Troubleshooting

### "Unable to load dynamic library"

- **Architecture Mismatch:** Ensure you didn't mix x64 PHP with x86 DLL (or vice versa).
- **Thread Safety Mismatch:** Ensure you didn't mix TS PHP with NTS DLL.
- **Missing fbclient.dll:** The Firebird client library must be in the PATH or PHP directory.
- **Visual C++ Redistributable:** Ensure you have the [latest Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) installed.

### "The specified module could not be found"

This usually means `fbclient.dll` cannot be found. Try copying `fbclient.dll` directly next to `php.exe`.

### Version Compatibility

- **Firebird 3.0 DLL:** Works with Firebird 3.0+ servers.
- **Firebird 4.0 DLL:** Works with Firebird 4.0+ servers (supports new data types).
- **Firebird 5.0 DLL:** Works with Firebird 5.0+ servers (supports latest features).

We recommend using the DLL version that matches your Firebird server version for best compatibility.
