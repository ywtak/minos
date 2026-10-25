# Putting minios on a Kindle 5th gen (2012, non-touch)

> **Read this first.** minios has been built and checked on a PC but **not yet run on a real Kindle**. Several device values (key codes, disk offsets, the USB-detect file) come from community documentation and must be confirmed on *your* unit; that is what steps B–C are for. Replacing the bootloader (step G) can brick the device permanently unless USB recovery works, so it is locked until step D succeeds. Go slowly, and stop at any step that does not behave as described.

What you need: the Kindle, charged above 50%; a USB cable; a Windows 11 PC with WSL2. Expect about 2 hours, most of it waiting for builds and copies.

---

## A. Build minios on the PC (once)

1. Install Ubuntu in WSL (PowerShell as administrator; reboot if asked):
   `wsl --install -d Ubuntu`
2. In the Ubuntu terminal:
   ```sh
   cp -r /mnt/c/Users/<you>/Desktop/minos ~/minos
   cd ~/minos
   make toolchain          # zig, Linaro gcc 4.9, apt packages (asks for your password)
   make test host          # unit test + PC version of the UI
   ```
   Optional check: put a few `.epub`/`.pdf` files in `~/minos/us/books/`, then run `build/minios-host`. Type `d`, `s`, `e`, `n`, `h`, `q` (and Enter) and open `screen.pgm` to see the screen.
3. Get Amazon's GPL source for the kernel and u-boot. On amazon.com, search for **"Kindle Source Code Notice"**, find **Kindle (5th Generation)** for **your firmware version** (on the Kindle: Menu → Settings → Menu → Device Info), download the tarball and save it as `~/minos/dl/kindle-src.tar.gz`.
4. Find the board configs and build:
   ```sh
   make list-configs                         # prints candidate kernel / u-boot configs
   make kernel KERNEL_DEFCONFIG=<the imx50 one it printed>
   make uboot  UBOOT_CONFIG=<the imx50 one it printed>
   make release
   ```
   Result: **`dist/minios-release.zip`**, which unzips to the folder `minios/`. That folder is the finished product.

## B. Open up the stock Kindle and back it up

1. **Jailbreak** firmware 4.1.x. Use the "Kindle 4 / Kindle 5 (non-touch) jailbreak" thread in MobileRead's *Kindle Developer's Corner*, choosing the package for your exact firmware version.
2. Install the **USBNetwork** hack from the same forum and enable it. This gives SSH over the USB cable: Kindle `192.168.15.244`, PC `192.168.15.201`, user `root`.
3. Plug the Kindle in (USB drive mode). Copy the unzipped `minios` folder onto the drive and rename it **`minios-flash`**, so it ends up at `/mnt/us/minios-flash` on the device.
4. Switch to USBNetwork and log in from WSL: `ssh root@192.168.15.244`. Then run:
   ```sh
   sh /mnt/us/minios-flash/discover.sh        # read-only
   /mnt/us/minios-flash/minios keys           # press every key once, then Ctrl+C
   ```
   Note the key codes printed. `discover.sh` wrote `/mnt/us/minios-facts.txt` and `/mnt/us/layout.env.suggested`.
5. **Back up everything to the PC.** Run this from a second WSL window, not on the Kindle:
   ```sh
   K=root@192.168.15.244; BB=/mnt/us/minios-flash/busybox
   mkdir -p ~/kindle-backup && cd ~/kindle-backup
   ssh $K cat /mnt/us/layout.env.suggested > layout.env.suggested
   . ./layout.env.suggested                       # gives KERNEL_OFFSET, KERNEL_MAX
   ssh $K "dd if=/dev/mmcblk0 bs=1M" > kindle-full.img
   ssh $K "dd if=/dev/mmcblk0 bs=512 count=$((KERNEL_OFFSET/512))" > boot-area.img
   ssh $K "dd if=/dev/mmcblk0 bs=512 skip=$((KERNEL_OFFSET/512)) count=$((KERNEL_MAX/512))" > kernel-slot.img
   ssh $K "dd if=/dev/mmcblk0p1 bs=1M" > p1.img
   ssh $K "$BB sha256sum /dev/mmcblk0"; sha256sum kindle-full.img   # the two hashes must match
   ```
   Keep `~/kindle-backup` safe, and make a second copy on another drive. It is your only way back.

## C. Write down the device facts

1. Send `minios-facts.txt` and your key codes back to me (or edit the files yourself). They go into **NOTES.md**, with status changed from DOC/TBD to VERIFIED. Wrong values get fixed in **config.h**: the key codes, `USERSTORE_DEV`, and `USB_ONLINE_PATHS`. If anything in config.h changed, rebuild with `make release` and copy the folder again.
2. Create **`layout.env`** in `minios-flash/`. Start from `layout.env.suggested` and check each line against `minios-facts.txt`:
   - `KERNEL_OFFSET`: the first `uImage at` line. Its name should look like the main kernel, not "diags".
   - `KERNEL_MAX`: the distance to the next uImage. `build/uImage` must be smaller than this.
   - `ROOTFS_PART=/dev/mmcblk0p1`. `ROOTFS_MAX` is its size; `build/rootfs.img` (96 MB) must be smaller.
   - `UBOOT_OFFSET=1024`. `UBOOT_MAX` = stock u-boot length − 1024.
   If any value is empty or `UNKNOWN`, **stop** and ask.

## D. USB recovery check (strongly recommended before E, required before G)

Without serial pads, the only way to rescue a Kindle that won't boot is the i.MX **USB serial-downloader mode**.
1. In WSL: `make imx_usb`.
2. Follow MobileRead's **"USB debricking for noobs (K4/K5)"** and **Kubrick** threads to find how *your* unit enters USB-downloader mode. On Windows, the device appears as a Freescale/"SE Blank" USB device (vendor `15a2`).
3. The check has passed when the PC sees that device **and** `build/imx_usb` can load the **stock** u-boot from `~/kindle-backup/boot-area.img` into RAM and boot it (the Kindle starts normally). Nothing is written to the eMMC during this check.
4. If it passed, create an empty file `minios-flash/RECOVERY_VERIFIED`. If you could not get it working, you may still do stage 1 (E) at your own risk, but **never do G**.

## E. Stage 1: install minios (stock bootloader kept)

On the Kindle over SSH:
```sh
sh /mnt/us/minios-flash/flash.sh stage1
```
The script checks the checksums, the slot sizes and the kernel magic, and asks you to type `FLASH`. It then freezes the filesystems, writes the kernel and rootfs, reads both back to compare, and reboots.
The Kindle should start into **All books**. The first start shows "No books yet".

If the screen stays blank or garbled: plug in USB. If the drive appears, open `.minios/log.txt` (it lists the framebuffer facts and any errors) and send it to me. If the drive does not appear either, use recovery (H2).

## F. Daily use and updates

- Plug into any PC. The Kindle shows *USB connected* and appears as a drive. Copy `.epub` / `.pdf` files into **`books/`**, eject, and unplug. The covers appear after a short "Preparing covers...".
- 5-way moves the selection, the page keys turn grid pages, and centre opens a book. In a book, the page keys or the 5-way turn pages, and Home/Back/Menu returns to the grid. The position is saved on every page.
- Power turns the device off; press power again to start.
- If a key does nothing: create an empty file `.minios/keys` on the drive and reboot. The screen then shows each key's code. Fix config.h, delete `.minios/keys`, and install the update as below.
- **Updating minios:** copy a new release into `minios-flash/` (keep `layout.env`, `RECOVERY_VERIFIED` and `backup/`), create an empty file `minios-flash/DO_STAGE1`, eject, unplug, and restart. The log is in `minios-flash/flash.log`.

## G. Stage 2: replace the bootloader (only after D passed)

1. Copy the current release into `minios-flash/` (keep `layout.env` and `RECOVERY_VERIFIED`).
2. Create an empty file **`minios-flash/DO_STAGE2`**, eject, unplug, and hold Power to restart.
3. minios runs `flash.sh stage2` at boot. It checks the IVT and size, writes `u-boot.bin` from 0x400, reads it back, and reboots. The result is in `minios-flash/flash.log`.
4. After this, nothing from Amazon's boot chain remains: the bootloader, kernel and system are all minios builds.

## H. Going back to stock

1. **minios still boots:** put `boot-area.img`, `kernel-slot.img` and `p1.img` from `~/kindle-backup` into `minios-flash/backup/`. Create `minios-flash/DO_RESTORE`, unplug, and restart. minios writes them back, reads them back to compare, and reboots into the stock firmware.
2. **Nothing boots:** enter USB-downloader mode (step D). Use `imx_usb` to start a u-boot from RAM, then write `kindle-full.img` back as the MobileRead debricking guide describes.
