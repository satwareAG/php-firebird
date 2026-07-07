# Windows Installation Guide for php-firebird

This guide explains how to install the `php-firebird` extension on Windows.

## 1. Download the Extension

Go to the [Releases page](https://github.com/satwareAG/php-firebird/releases) and download the DLL that matches your environment.

The DLL filenames follow this convention:
`php_firebird-{version}-php{php_version}-{ts|nts}-{arch}-fb{fb_version}.dll`

**You need to match 4 criteria:**

1.  **PHP Version**: 8.1, 8.2, 8.3, or 8.4
2.  **Thread Safety**:
    *   `ts` (Thread Safe) - usually for Apache
    *   `nts` (Non Thread Safe) - usually for IIS or FastCGI
    *   *Check `php -i | findstr "Thread Safety"` to be sure.*
3.  **Architecture**:
    *   `x64` (64-bit) - standard for modern systems
    *   `x86` (32-bit) - legacy
    *   *Check `php -i | findstr "Architecture"` to be sure.*
4.  **Firebird Client Version**:
    *   `fb3.0` - for Firebird 3.0 client
    *   `fb4.0` - for Firebird 4.0 client
    *   `fb5.0` - for Firebird 5.0 client

**Example:**
If you have PHP 8.3 (Thread Safe) x64 and want to use Firebird 5.0 client, download:
`php_firebird-12.0.0-php8.3-ts-x64-fb5.0.dll`

## 2. Install the Extension

1.  **Rename the file**: Rename the downloaded file to `php_firebird.dll`.
2.  **Move to extensions directory**: Copy `php_firebird.dll` to your PHP `ext` directory (e.g., `C:\php\ext`).
3.  **Edit php.ini**: Add the following line to your `php.ini` file:
    ```ini
    extension=php_firebird
    ```

## 3. Install Firebird Client Library

The extension requires the Firebird client library (`fbclient.dll`) to be available in your system PATH or in the same directory as `php.exe`.

1.  Download the **Firebird Windows ZIP kit** (not the installer) matching your Firebird version (3.0, 4.0, or 5.0) and architecture (x64 or x86) from the [Firebird website](https://firebirdsql.org/en/downloads/).
2.  Extract `fbclient.dll` from the ZIP file.
3.  Copy `fbclient.dll` to your PHP root directory (where `php.exe` is located).

**Note:** If you already have the Firebird Server installed on the same machine, the client library might already be in your PATH. However, copying it to the PHP directory ensures the correct version is used.

## 4. Verify Installation

Open a command prompt and run:

```cmd
php -m
```

You should see `firebird` in the list of modules.

To check detailed information:

```cmd
php -ri firebird
```

## Troubleshooting

### "Unable to load dynamic library"

*   **Architecture Mismatch**: Ensure you didn't mix x64 PHP with x86 DLL (or vice versa).
*   **Thread Safety Mismatch**: Ensure you didn't mix TS PHP with NTS DLL.
*   **Missing fbclient.dll**: Ensure `fbclient.dll` is in the PHP directory or PATH.
*   **Visual C++ Redistributable**: Ensure you have the [Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) installed.

### "The specified module could not be found"

This usually means `fbclient.dll` is missing or incompatible.

### "%1 is not a valid Win32 application"

This means you are trying to load a 64-bit DLL into a 32-bit PHP (or vice versa).
