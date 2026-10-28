# minios device notes: Kindle 5th gen (2012, non-touch)

Every hardware fact minios relies on, and how sure we are of it.
**Status:** `VERIFIED` = seen on this unit · `DOC` = from community docs or source, not yet seen on this unit · `TBD` = fill in from `minios-facts.txt` (written by `scripts/discover.sh`).
When a value is confirmed, change its status here and fix `config.h` or `layout.env` to match.

## Hardware
| Fact | Value | Status |
|---|---|---|
| SoC | Freescale i.MX508, ARM Cortex-A8 (VFPv3/NEON) | DOC (user) |
| Display | 600×800 e-ink, i.MX508 EPDC + Lab126 `mxc_epdc_fb` driver | DOC (user) |
| Input | physical keys only: 5-way, page keys both sides, Home, Menu, Back, Keyboard, Power | DOC (user) |
| Unit | serial prefix `9023` = Kindle 5th gen (2012, black); USB `1949:0004` | VERIFIED (USB descriptor) |
| Stock firmware | 4.1.4, Linux 2.6.31 (Lab126) | VERIFIED (user, Settings) |
| Board | "Tequila" (board id `003`, EVT2 `00309`); kernel `imx50_tequila_defconfig` (MACH_MX50_YOSHI), u-boot `imx50_yoshi_config` | DOC (Amazon GPL source): check `board` section |
| RAM | 256 MB (check `meminfo`) | TBD |
| Storage | eMMC `/dev/mmcblk0` | DOC |

## Boot and eMMC layout
| Fact | Value | Status |
|---|---|---|
| Boot ROM loads the image from eMMC; i.MX IVT at byte 0x400 (tag `d1`) | `UBOOT_OFFSET=1024` | DOC (i.MX50 reference manual): check `ivt` section |
| Stock u-boot length (IVT boot_data) | → `UBOOT_MAX` | TBD |
| Main kernel: uImage (magic `27 05 19 56`) at a raw offset | → `KERNEL_OFFSET` | TBD (`uimage-scan`) |
| Kernel slot size (up to the next uImage, which is likely the diags kernel) | → `KERNEL_MAX` | TBD |
| Root filesystem partition | `/dev/mmcblk0p1` (ext3) | DOC, TBD size → `ROOTFS_MAX` |
| User storage ("USB drive") partition | `/dev/mmcblk0p4` (FAT) → `USERSTORE_DEV` in config.h | DOC, TBD |
| Bytes between u-boot and kernel may hold idme data (serial, board id) and the e-ink waveform | never written by minios except during restore | DOC: confirm before stage 2 |
| Kernel command line | built into our kernel: `consoleblank=0 rootwait ro ip=off root=/dev/mmcblk0p1 debug eink=fslepdc video=mxcepdcfb:E60,bpp=8` | DOC (defconfig); stock u-boot may override: check `cmdline` |

## Display (mxcfb)
| Fact | Value | Status |
|---|---|---|
| `/dev/fb0` 8 bpp grayscale, 0 = black | `FB_INVERT 0` | DOC: minios logs the real fb info to `.minios/log.txt` |
| Refresh ioctl `MXCFB_SEND_UPDATE = _IOW('F', 0x2E, struct mxcfb_update_data)` | eink.c tries the Lab126 struct with the `hist_*` fields first, then the short one; the log says which worked | DOC (KOReader `mxcfb-kindle.h`) |
| `WAVEFORM_MODE_AUTO` 257, `UPDATE_MODE_PARTIAL/FULL` 0/1, `TEMP_USE_AMBIENT` 0x1000 | config.h | DOC (KOReader) |
| Waveform data: the Lab126 driver loads it from eMMC itself | none needed from us | DOC: confirm the panel works after stage 1 |

## Keys (evdev codes)
Confirm with `./minios keys` over SSH on stock firmware (or create `.minios/keys` on the USB drive under minios).

| Key | Code in config.h | Status |
|---|---|---|
| 5-way up/down/left/right | 103 / 108 / 105 / 106 | DOC (standard evdev) |
| 5-way centre | 194 | DOC (MobileRead keymap for K3/K4) |
| Page forward | 191 | DOC (MobileRead) |
| Page back | 193 | DOC (MobileRead) |
| Home / Back / Menu | 102 / 158 / 139 | DOC |
| Power | 116 | DOC |

## USB and power
| Fact | Value | Status |
|---|---|---|
| Mass-storage gadget | `g_file_storage file=/dev/mmcblk0p4` (2.6.31 has no `g_mass_storage`) | DOC: check `usb-gadget` section |
| Cable-detect file | first of `USB_ONLINE_PATHS` in config.h that exists | TBD (`power-supply` section) |
| `/proc/sysrq-trigger` | absent on stock 4.1.4 (no MAGIC_SYSRQ); flash.sh skips the freeze because `/` is mounted ro; our kernel enables it | DOC (defconfig) |
| Charging gate | `arcotg_udc` only charges if the battery id is in `tequila_valid_ids` {12, 76, 140, 204} | DOC (`battery_id.h`): check `battery` section |
| Driver modules (all `=m`, must be loaded by `/etc/modules`) | e-ink: `eink_fb_waveform eink_fb_hal eink_fb_shim eink_fb_hal_fslepdc mxc_epdc_fb`; keys: `mxc_keyb tequila_keypad fiveway`; USB: `arcotg_udc g_file_storage` | DOC (our build): order from `modules` section |
| Suspend to RAM | screensaver writes `mem` to `/sys/power/state` (`SUSPEND_PATH`); power key wakes; log shows `suspend: resumed` or the error | DOC (stock powerd does the same): TBD |
| CPU idle clocking | rc.local sets cpufreq `ondemand` and `mxc_dvfs_core.0/enable` if present | DOC: TBD |
| User storage size | 1.45 GB FAT32, label `Kindle` | VERIFIED (Windows) |
| USBNetwork hack addresses | Kindle 192.168.15.244, PC 192.168.15.201 | DOC (hack default; check its config) |

## Recovery
| Fact | Value | Status |
|---|---|---|
| No serial console (user won't solder) | so recovery must be USB-only | user decision |
| i.MX USB serial-downloader mode, used by `imx_usb_loader` and MobileRead's "Kubrick" | a no-solder way to enter it on this unit is **unconfirmed**; one 2014 MobileRead report says holding power then DOWN did not work on a K4NT | TBD: see INSTALL.md, troubleshooting |
| Stage 2 (u-boot) is allowed only after step F succeeds | enforced by `RECOVERY_VERIFIED` in flash.sh | design |

## Source
- Amazon GPL source: `https://s3.amazonaws.com/kindledownloads/Kindle_src_4.1.4_3543630001.tar.gz`. Older releases are at `https://kindle.s3.amazonaws.com/Kindle_src_<ver>.tar.gz`.
- The 4.1.4 kernel tarball lacks `boardid.h`, `battery_id.h`, `llog.h`, `linux/einkfb.h` and `linux/kindle_version.h`. The first four are copied from `Kindle_src_5.0.0` into `kernel/`. `boardid.h` adds the YOSHI/FINKLE ids that 4.1.4 still references. `kindle_version.h` is ours.
- Toolchain: an Ubuntu 14.04 chroot with gcc 4.7 (`make toolchain`). Newer gcc and make fail on this kernel.

## Software constraints
- The EPDC driver exists only in Amazon's GPL kernel (2.6.31); mainline Linux has none for i.MX508. We build our own kernel from that source.
- glibc ≥ 2.26 refuses kernels < 3.2, so userspace is static musl (zig 0.13, `arm-linux-musleabihf`, Cortex-A8). musl's `system()` uses `pipe2` (kernel ≥ 2.6.27): OK.
- `devtmpfs` arrived in 2.6.32, so `/dev` is a tmpfs filled by `mdev -s`.
- Busybox has no `fsck.vfat`. If the PC is unplugged without ejecting, Linux still mounts the dirty FAT volume.
- minios mounts `/` read-only; all state (positions, cover cache, log) is on the USB drive under `.minios/`.

## Log
- 2026-10-22: project created. No device facts verified yet; every value above is DOC or TBD.
- 2026-10-22: unit identified over USB (serial 9023…, fw 4.1.4). Kernel 2.6.31-rt11-lab126 (uImage 3.76 MB) and u-boot (64 KB) build from the 4.1.4 source.
