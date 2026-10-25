
# NovaeOS 🖥️

**A 32-bit x86 Operating System Kernel** — written from scratch in C and NASM assembly, with a Windows-style graphical desktop and a text-mode shell.

---

## Features

| Component | Details |
|-----------|---------|
| **Bootloader** | GRUB Multiboot (x86 NASM assembly) |
| **Architecture** | i686 (32-bit Protected Mode) |
| **GDT** | 5 descriptors: null, kernel code/data, user code/data |
| **IDT** | 256 gates: 32 CPU exceptions + 16 IRQs |
| **PIC** | 8259 remapped to INT 32-47 |
| **Timer** | PIT 8253/8254 @ 100 Hz |
| **Keyboard** | PS/2 scan set 1, shift, caps lock, ring buffer |
| **Memory** | Bitmap physical allocator + first-fit heap |
| **Filesystem** | NovaeVFS -- in-memory flat filesystem |
| **Shell** | NovaeShell -- 25+ built-in commands |
| **Graphics** | 1024x768 32-bit linear framebuffer (GRUB VBE or Bochs/QEMU BGA), double buffered |
| **Mouse** | PS/2 mouse on IRQ12 |
| **Desktop** | Wallpapers, desktop icons, taskbar, Start menu with search, clock + calendar |
| **Windows** | Drag, focus, minimize, maximize, resize, close |
| **Apps** | Calculator, Notepad, File Explorer, Terminal, Settings |

---

## Desktop

The default boot entry starts the graphical desktop:

- **Start menu** (click the NovaeOS logo or press the Windows key): pinned apps, type-to-search, recent files, power menu (restart / shut down)
- **Taskbar**: pinned apps with running indicators, clock (click for a calendar), show-desktop corner
- **Desktop**: double-click icons to open them, right-click for a context menu
- **Calculator**: operator precedence, parentheses, square, square root, percent, keyboard input
- **Notepad**: edit and save files in NovaeVFS
- **File Explorer**: browse, create, open and delete files
- **Terminal**: runs every shell command (`help`, `ls`, `cat`, `fetch`...); `calc`, `notepad` and `settings` open the desktop apps
- **Settings**: wallpaper (Flow, Aurora, Sunset, Midnight, Solid color), accent color, light/dark mode, taskbar alignment, system info, about

The text-mode shell is still available from the **"NovaeOS 1.0.0 (Text mode shell)"** GRUB entry (kernel argument `text`).

---

## Shell Commands

```
System:       help  clear  uname  uptime  meminfo  reboot  shutdown
Filesystem:   ls  cat  write  touch  rm  mv  cp
Apps:         calc  notepad  clock  calendar  sysmon  settings  fetch
Fun:          echo  banner  color  matrix  snake
```

---

## File Structure

```
d:/OS/
+-- boot/
|   +-- boot.asm          <- Multiboot entry point
+-- kernel/
|   +-- kernel.c          <- Main kernel entry
|   +-- cpu/
|   |   +-- gdt.c/h       <- Global Descriptor Table
|   |   +-- gdt_flush.asm <- GDT reload (assembly)
|   |   +-- idt.c/h       <- Interrupt Descriptor Table
|   |   +-- isr.asm       <- All 48 ISR/IRQ stubs
|   +-- drivers/
|   |   +-- vga.c/h       <- VGA text mode 80x25
|   |   +-- timer.c/h     <- PIT 8253 timer
|   |   +-- keyboard.c/h  <- PS/2 keyboard
|   |   +-- mouse.c/h     <- PS/2 mouse
|   |   +-- rtc.c/h       <- CMOS real-time clock
|   +-- gfx/
|   |   +-- gfx.c/h       <- Framebuffer, drawing primitives, cursor
|   |   +-- font8x16.c    <- Terminus 8x16 bitmap font (generated)
|   +-- gui/
|   |   +-- wm.c          <- Window manager + event loop
|   |   +-- desktop.c     <- Wallpaper, icons, taskbar, Start menu
|   |   +-- icons.c       <- Procedural icons and logo
|   |   +-- widgets.c     <- Buttons, text boxes
|   |   +-- apps/         <- calc, notepad, files, terminal, settings
|   +-- mm/
|   |   +-- pmm.c/h       <- Physical memory manager
|   |   +-- heap.c/h      <- Kernel heap allocator
|   +-- fs/
|   |   +-- vfs.c/h       <- Virtual filesystem
|   +-- lib/
|   |   +-- kstring.c/h   <- String utilities (no libc)
|   |   +-- mem.c         <- memcpy/memset for the compiler
|   +-- shell/
|       +-- shell.c/h     <- Interactive shell + all apps
+-- linker.ld             <- Kernel memory layout
+-- Makefile              <- Build system
+-- Dockerfile            <- Docker build environment
+-- build.ps1             <- Windows build helper
+-- tools/gen_font.py     <- Regenerates kernel/gfx/font8x16.c
+-- iso/boot/grub/grub.cfg  <- GRUB configuration
```

---

## Building and Running

### Option 1: Docker + QEMU (Recommended for Windows)

Requirements:
- Docker Desktop: https://www.docker.com/products/docker-desktop
- QEMU for Windows: https://www.qemu.org/download/#windows

Build:
```powershell
# In PowerShell, from d:\OS\
.\build.ps1
```

### Option 2: WSL2 (Windows Subsystem for Linux)

```bash
# Install tools inside WSL2 Ubuntu
sudo apt install gcc nasm binutils make qemu-system-x86 gcc-multilib

# Build and run
cd /mnt/d/OS
make run
```

### Option 3: Native Linux / macOS

Install dependencies:
```bash
# Ubuntu/Debian
sudo apt install build-essential nasm gcc-multilib binutils qemu-system-x86

# macOS
brew install nasm qemu
```

Build:
```bash
make all          # Compile kernel binary
make run          # Launch in QEMU immediately
make iso          # Create bootable ISO
make run-iso      # Boot from ISO in QEMU
make run-debug    # Start with GDB remote server (:1234)
make clean        # Remove build artifacts
```

### Option 4: Cross-Compiler (Most Correct)

Build an i686-elf cross-compiler (see: https://wiki.osdev.org/GCC_Cross-Compiler)
Then the Makefile will automatically use i686-elf-gcc and i686-elf-ld.

---

## QEMU Run Commands

```bash
# Basic run (after building)
qemu-system-i386 -kernel build/kernel.bin -m 128M

# With serial console output
qemu-system-i386 -kernel build/kernel.bin -m 128M -serial stdio

# Boot from ISO
qemu-system-i386 -cdrom novaeos.iso -m 128M

# Enable KVM acceleration (Linux only)
qemu-system-i386 -kernel build/kernel.bin -m 128M -enable-kvm
```

---

## Boot Flow

```
Power On
  +-- BIOS POST
       +-- GRUB Bootloader
            +-- Multiboot header detected
                 +-- boot.asm (_start)
                      +-- Set up stack (16 KB)
                      +-- Call kernel_main(magic, mbinfo)
                           +-- vga_init()
                           +-- gdt_init()    lgdt
                           +-- idt_init()    lidt + PIC remap
                           +-- pmm_init()    bitmap allocator
                           +-- heap_init()   1MB kernel heap
                           +-- timer_init()  PIT @ 100 Hz
                           +-- keyboard_init()
                           +-- vfs_init()    NovaeVFS
                           +-- sti()         interrupts on
                           +-- shell_run()   REPL loop
```

---

## Adding New Commands

1. Add your function in kernel/shell/shell.c
2. Register it in the dispatch() function
3. Add a help entry in cmd_help()
4. Rebuild with: make

---

## License

MIT License - build, learn, and modify freely!
