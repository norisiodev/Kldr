
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

1. Install the EDK II on the linux system.

2. Change current directory to edk2.

3. Set up the edk2 build environment.

    Run following command with bash.

    ``` sh
    . edksetup.sh
    ```


4. Modify following definitions in the "Conf/target.txt".
    ```
    ACTIVE_PLATFORM       = KldrPkg/KldrPkg.dsc
    TARGET                = RELEASE
    TARGET_ARCH           = X64
    TOOL_CHAIN_TAG        = GCC5
    ```

5. Clone Kldr repository and rename the Kldr to KldrPkg.
    ``` sh
    git clone https://github.com/norisiodev/Kldr.git
    mv Kldr KldrPkg
    ```

7. Build Kldr.efi.
    ``` sh
    build
    ```

