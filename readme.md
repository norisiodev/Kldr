
# Kldr

This is the UEFI application that loads the linux kernel.

The loader supports following features.

- bzImage (only x86_64)
- initial ram disk
- kernel parameter

## How it works.

Kldr.efi loads the bzImage, initial ram disk and kernel parameter file from the root directory that same filesystem.

## How to use.

Create an ESP(EFI system partition) and prepare linux kernel and initial ram disk in advance.

Partition editor that can create ESP : [GParted](https://gparted.org/)

Linux kernel and initial ram disk : [debian](https://www.debian.org/distrib/) / [direct link to files](https://deb.debian.org/debian/dists/bullseye/main/installer-amd64/current/images/hd-media/)

Note: The following is an execution example.

1. Rename the kernel image file to "bzimage".

    ``` sh
    cp /boot/vmlinuz-5.10.0-11-amd64 bzimage
    ```
2. Rename the initial ram disk file to "initrd".

    ``` sh
    cp /boot/initrd.img-5.10.0-11-amd64 initrd
    ```
3. Create a "config.txt" containing the kernel parameters.

    ``` sh
    echo "root=/dev/sda2" > config.txt
    ```
4. Copy "bzimage", "initrd", "config.txt" to the same root directry as "Kldr.efi" on the ESP(EFI system partition).

    ```
    FS0:
    +-- EFI
        +-- BOOT
            +-- Kldr.efi
    +-- bzimage
    +-- initrd
    +-- config.txt
    ```

5. Run the "Kldr.efi" to boot the kernel or install it to the UEFI Boot Option.
    - Boot the kernel.

        ``` efi
        FS0:\EFI\BOOT\Kldr.efi boot
        ```
    - Install to the UEFI Boot Option.

        ``` efi
        FS0:\EFI\BOOT\Kldr.efi install
        ```

        After reboot the computer, "Kldr.efi" will be executed automatically.

## How to build.

1. Install the EDK II on the linux, intel mac or Windows VS2019.

    edk2:[https://github.com/tianocore/edk2](https://github.com/tianocore/edk2)

2. Change current directory to edk2.

3. Set up the edk2 build environment.

    - linux

        Run following command with bash or zsh.

        ``` sh
        source edksetup.sh
        ```

    - mac

        Run following commands with bash or zsh.

        ``` sh
        source edksetup.sh
        cd BaseTools\Source\C
        make
        cd ..\..\..
        ```

    - Windows

        Run following commands with "x86 Native Tools Command Prompt for VS 2019"
        ``` sh
        edksetup.bat
        cd BaseTools\Source\C
        nmake
        cd ..\..\..
        ```


4. Modify following definitions in the "Conf/target.txt".
    ```
    ACTIVE_PLATFORM       = KldrPkg/KldrPkg.dsc
    TARGET                = RELEASE
    TARGET_ARCH           = X64
    TOOL_CHAIN_TAG        = GCC5        # for linux
    TOOL_CHAIN_TAG        = XCODE5      # for intel mac
    TOOL_CHAIN_TAG        = VS2019      # for Windows
    ```

5. Clone Kldr repository and rename the Kldr to KldrPkg.

    linux or mac
    ``` sh
    git clone https://github.com/norisiodev/Kldr.git
    mv Kldr KldrPkg
    ```

    Windows
    ``` sh
    git clone https://github.com/norisiodev/Kldr.git
    ren Kldr KldrPkg
    ```

7. Build Kldr.efi.
    ``` sh
    build
    ```

