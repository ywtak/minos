# Installing minios, step by step

This guide starts from a **stock Kindle that has never been jailbroken** and ends with minios running on it. It also covers updating minios and going back to Amazon's software.

> **Read the whole guide once before you start.** Step 7 overwrites the Kindle's system partition. The backup in step 4 is your only way back, so do not skip it and do not lose it. minios has been tested on one unit only. Go slowly, and stop at any step that does not behave as described.

**You need:**
- A Kindle 5th generation (2012, non-touch, 5-way pad, no keyboard), charged above 50%
- A USB cable
- A Windows 10/11 PC (Linux works too; skip the WSL parts)
- About 3 GB of free disk space, and 1–2 hours

---

## 1. Check your Kindle

On the Kindle: **Home → Menu → Settings → Menu → Device Info**.

- **Serial number** should start with `9023` (Kindle 5th gen, black, 2012), the model minios was tested on. The older silver Kindle 4 is closely related but untested; the layout check in step 4.4 tells you if yours differs.
- **Firmware version** must be **4.1.x** (minios was developed on 4.1.4). If it says 4.0.x, update to 4.1.4 first, using Amazon's "Kindle (5th Generation) Software Update".

Write the serial number down. You may need it for the root password in step 3.

## 2. Jailbreak

The jailbreak only installs a developer key, so the Kindle accepts the USBNetwork package in step 3. Full instructions and the download are on the MobileRead wiki: <https://wiki.mobileread.com/wiki/Kindle4NTHacking#Jailbreak>, which links to the jailbreak thread <https://www.mobileread.com/forums/showthread.php?t=191158>.

1. Download the Kindle 4 jailbreak zip from that thread and unzip it on the PC.
2. Plug the Kindle into the PC. It appears as a drive named **Kindle**.
3. Copy **`data.tar.gz`**, **`ENABLE_DIAGS`** and the **`diagnostic_logs`** folder from the zip to the **root** of the Kindle drive.
4. Eject the drive in Windows and unplug the cable.
5. Restart the Kindle: **Menu → Settings → Menu → Restart**. It starts in *diagnostics mode* (a text menu).
6. With the 5-way pad, select **D) Exit, Reboot or Disable Diags**, then **R) Reboot System**, then **Q) To continue**. When the screen asks for **FW Left**, press **left** on the 5-way.
7. Wait about 20 s. The jailbreak screen appears, then the Kindle restarts normally.
8. **Check:** your library now contains a book called **"You are Jailbroken"**.

If the Kindle stays in diagnostics mode, first run any single test from the diagnostics menu (this writes `diagnostic_logs/device_info.xml`), then exit again as in step 6.

## 3. Install USBNetwork (SSH over the USB cable)

1. Download **`kindle-usbnetwork-0.57.N-k4.zip`** from <https://www.mobileread.com/forums/showthread.php?t=88004> (in the attached files list). Unzip it and read its `README_FIRST.txt`.
2. Plug the Kindle in and copy the **`Update_usbnetwork_*_k4_install.bin`** file from the zip to the root of the Kindle drive. Eject and unplug.
3. On the Kindle: **Home → Menu → Settings → Menu → Update Your Kindle**, then confirm. It installs and restarts.
4. Turn USBNetwork on. On the Home screen, press the **Keyboard** key to open the search box, then type each line and press Enter after it:
   ```
   ;debugOn
   ~usbNetwork
   ;debugOff
   ```
   From now on, while USBNetwork is on, the cable is a network link instead of a USB drive. Typing `~usbNetwork` again (after `;debugOn`) switches it back to drive mode. Restarting the Kindle also switches it back.
5. **Windows network setup.** Plug the cable in. Windows finds a new network adapter (*RNDIS* / *USB Ethernet*).
   - If Device Manager lists it with a warning sign, right-click it and choose **Update driver → Browse my computer → Let me pick → Network adapters → Microsoft → Remote NDIS Compatible Device**.
   - Then go to **Control Panel → Network and Internet → Network Connections**, right-click the new adapter, and open **Properties → Internet Protocol Version 4**. Set the IP address to **`192.168.15.201`** and the subnet mask to **`255.255.255.0`**, and leave the gateway empty.
6. **Log in.** In PowerShell: `ssh root@192.168.15.244`.
   - If it asks for a password, it is the Kindle's root password, which is derived from the serial number. Get it from KindleTool (`kindletool info <serial>`) or the calculator linked on the MobileRead wiki page above. If the Kindle was ever restored from a debricking image, try `mario`.
   - To log in without a password, put your public key in `usbnet/etc/authorized_keys` on the Kindle drive (see the USBNetwork README).
   - You should get a `[root@kindle root]#` prompt. Type `exit` to leave.

## 4. Install WSL and make the full backup

The build and the backup both run in Ubuntu under WSL2.

1. Open PowerShell **as administrator**, run `wsl --install -d Ubuntu`, and reboot if asked. Open **Ubuntu** from the Start menu and create a user.
2. In Ubuntu, get minios and its tools. `make toolchain` asks for your password and downloads about 100 MB.
   ```sh
   sudo apt-get update && sudo apt-get install -y git make
   git clone https://github.com/ywtak/minos.git ~/minos
   cd ~/minos
   make toolchain            # zig, genext2fs, e2fsprogs, zip
   make third_party/busybox  # a busybox for the Kindle (the stock one lacks tools)
   ```
3. Turn the Kindle back into **drive mode** (step 3.4, or just restart it), plug it in, and create a folder **`minios-flash`** on the Kindle drive. From Ubuntu, copy the helpers into it. Here `D:` is the Kindle drive's letter: change `/mnt/d` to match.
   ```sh
   cp third_party/busybox scripts/discover.sh /mnt/d/minios-flash/
   ```
   Eject, unplug, turn **USBNetwork on** again (step 3.4), and plug the cable back in.
4. **Check that minios knows your Kindle's disk layout.** In Ubuntu:
   ```sh
   K=root@192.168.15.244
   ssh $K sh /mnt/us/minios-flash/discover.sh
   ssh $K "grep -e '^uImage at' -e mmcblk0p /mnt/us/minios-facts.txt"
   ```
   You should see `uImage at 266240 …`, `uImage at 14946304 …`, and partitions **`mmcblk0p1` = 358400** and **`mmcblk0p2` = 65536** (sizes in KiB). These are the values in `scripts/layout.env`. **If any of them differ, stop here** and open an issue with your `minios-facts.txt`. Remove the `Serial` line from it before posting.
5. **Back up the whole eMMC.** This takes 5–15 minutes and produces a ~2 GB file:
   ```sh
   mkdir -p ~/kindle-backup && cd ~/kindle-backup
   ssh $K "dd if=/dev/mmcblk0 bs=1M" > kindle-full.img
   ssh $K "/mnt/us/minios-flash/busybox sha256sum /dev/mmcblk0"
   sha256sum kindle-full.img
   ```
   **The two hashes must be identical.** If they are not, run the `dd` line again.
6. Copy `kindle-full.img` somewhere outside WSL as well, for example a USB stick or cloud storage: `cp kindle-full.img /mnt/c/Users/<you>/Documents/`. **This file is your only way back to Amazon's software.**

## 5. Build minios

Choose your version (see the table in [README.md](README.md)):

```sh
cd ~/minos
git checkout v1.2-games          # or v1.1-battery, or v1.0-bare
make test                        # unit tests
make release BACKUP=~/kindle-backup/kindle-full.img
```

The first build takes a while because it compiles MuPDF. The result is the folder **`dist/minios/`**, which contains `rootfs.img`, `minios`, `busybox`, `flash.sh`, `layout.env`, `SHA256SUMS` and these docs. The same folder is also zipped as `dist/minios-release.zip`.

Optional: run `make host`, put a few books in `~/minos/us/books/`, and run `build/minios-host`. Type `d`, `s`, `e`, `n`, `h`, `q` + Enter and look at `screen.pgm` to see the screen without a Kindle.

## 6. Copy the release to the Kindle

1. Put the Kindle in **drive mode** (restart it), then plug it in.
2. Copy the release over the helpers from step 4.3 (`D:` = the Kindle drive):
   ```sh
   cp -r ~/minos/dist/minios/. /mnt/d/minios-flash/
   ```
3. Copy some books: make a folder **`books`** on the Kindle drive and put `.epub` / `.pdf` files in it.
4. Eject, unplug, and turn **USBNetwork on** (step 3.4).

## 7. Install

```sh
ssh root@192.168.15.244
sh /mnt/us/minios-flash/flash.sh install
```

`flash.sh` checks the release checksums, the partition size and the kernel position, then asks you to type **`FLASH`**. It then:
- writes `rootfs.img` to the system partition and reads it back to compare,
- wipes the diagnostics system (no longer needed: minios keeps Amazon's kernel and bootloader),
- reboots.

If it stops with an error, nothing has been half-written without telling you. Read the message. A `READ-BACK MISMATCH` means: **do not reboot**, and ask for help.

## 8. First start and daily use

The Kindle starts into **All books**. The first start shows *Preparing covers…*.

- **Adding books:** plug into any PC. The Kindle shows *USB connected* and appears as a drive. Copy `.epub`/`.pdf` into `books/` (subfolders are fine), eject, and unplug.
- **Reading:** the 5-way picks a book and centre opens it. Page keys or the 5-way turn pages. Home/Back/Menu returns to the library. Your position is saved on every page.
- **Screensaver:** after one idle minute. Put `screensaver.png` (or `.jpg`) on the drive root to use your own picture.
- **Battery** (v1.1+): from the top row of books, press up to select the battery, then centre to see the time left.
- **Games** (v1.2): press Menu in the library.
- **Power:** the Power key switches the Kindle off. Press it again to start.

minios has no USBNetwork or SSH. From now on, the drive is how you talk to it.

## 9. Updating, switching versions, going back to stock

**Install another minios version or an update** (no SSH needed):
1. Build it: `cd ~/minos && git pull && git checkout <tag> && make release BACKUP=~/kindle-backup/kindle-full.img`.
2. Plug in the Kindle and copy `dist/minios/.` into `minios-flash/` as in step 6.
3. Create an empty file **`minios-flash/DO_INSTALL`**, then eject and unplug. minios installs the update when the cable comes out, reboots, and writes a log to `minios-flash/flash.log`.

**Back to Amazon's software:**
1. Cut the restore images out of your backup:
   ```sh
   cd ~/minos && make restore-images BACKUP=~/kindle-backup/kindle-full.img
   ```
   This writes `p1.img`, `p2.img`, `diags-kernel.img` and `SHA256SUMS` into `~/kindle-backup/`.
2. Plug in the Kindle and copy those four files into **`minios-flash/backup/`** on the drive.
3. Create an empty file **`minios-flash/DO_RESTORE`**, then eject and unplug. minios writes Amazon's system back, verifies it, and reboots into the stock Kindle software. The log is in `minios-flash/flash.log`.

## 10. Troubleshooting

| Problem | What to do |
|---|---|
| Blank or garbled screen after install | Plug in USB. If the drive appears, read `.minios/log.txt`: it records the display and input setup and any errors. Restore via `DO_RESTORE` (step 9) if needed. |
| A key does nothing | Best checked **before** step 7: over SSH on the stock Kindle, run `/mnt/us/minios-flash/minios keys`, press every key once (each code is printed), then Ctrl+C. Compare with the `K_*` codes in `config.h`, fix, and rebuild. Do not use the `.minios/keys` file on an installed minios: that mode never turns on USB, so you could not delete the file again without restoring. |
| Battery not shown (v1.1+) | `.minios/log.txt` lists the battery folders that were found. Set `BATTERY_DIR` in `config.h` to match. |
| A book has no cover / wrong title | Delete `.minios/covers/` on the drive. Covers are rebuilt at the next start. |
| Neither the screen nor the drive works | You need USB recovery (i.MX serial-download mode). See MobileRead's *"USB debricking for noobs (K4/K5)"* and *Kubrick* threads, then write `kindle-full.img` back. This is why step 4 matters. |

Device facts and how they were verified are in [NOTES.md](NOTES.md).
