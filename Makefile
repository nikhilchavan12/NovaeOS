# =============================================================================
# NovaeOS - Makefile
# Builds the OS kernel and creates a bootable ISO
# =============================================================================

# ---- Toolchain ---------------------------------------------------------------
# Cross-compiler targeting bare-metal i686 (no OS ABI assumptions)
# Install: i686-elf-gcc, i686-elf-ld, nasm, grub-mkrescue, xorriso
# On Ubuntu/Debian: sudo apt install build-essential nasm grub-pc-bin xorriso
# Or use Docker (see Dockerfile)

CC      := i686-elf-gcc
LD      := i686-elf-ld
AS      := nasm

# Fallback: use system GCC if cross-compiler not available (not recommended)
# CC    := gcc
# LD    := ld

# ---- Flags ------------------------------------------------------------------
CFLAGS  := -m32                  \
            -ffreestanding       \
            -fno-builtin         \
            -fno-stack-protector \
            -fno-pic             \
            -O2                  \
            -Wall                \
            -Wextra              \
            -nostdlib            \
            -nostdinc            \
            -std=c11             \
            -Ikernel

ASFLAGS := -f elf32

LDFLAGS := -m elf_i386           \
            -T linker.ld         \
            --oformat binary

# ---- Directories -------------------------------------------------------------
BUILD   := build
ISO_DIR := iso
GRUB_CFG:= $(ISO_DIR)/boot/grub/grub.cfg

# ---- Source files ------------------------------------------------------------
ASM_SRCS := boot/boot.asm              \
             kernel/cpu/gdt_flush.asm  \
             kernel/cpu/isr.asm

C_SRCS   := kernel/kernel.c            \
             kernel/drivers/vga.c      \
             kernel/drivers/timer.c    \
             kernel/drivers/keyboard.c \
             kernel/cpu/gdt.c          \
             kernel/cpu/idt.c          \
             kernel/mm/pmm.c           \
             kernel/mm/heap.c          \
             kernel/fs/vfs.c           \
             kernel/lib/kstring.c      \
             kernel/shell/shell.c

# ---- Object files -----------------------------------------------------------
ASM_OBJS := $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))
C_OBJS   := $(patsubst %.c,   $(BUILD)/%.o, $(C_SRCS))
ALL_OBJS := $(ASM_OBJS) $(C_OBJS)

KERNEL   := $(BUILD)/kernel.bin
ISO      := novaeos.iso

# ---- Targets -----------------------------------------------------------------
.PHONY: all clean iso run run-debug

all: $(KERNEL)
	@echo ""
	@echo "  ✓ NovaeOS kernel built: $(KERNEL)"
	@echo "  Run 'make iso' to create a bootable ISO"
	@echo "  Run 'make run' to launch in QEMU"
	@echo ""

# Link kernel binary
$(KERNEL): $(ALL_OBJS)
	@mkdir -p $(dir $@)
	@echo "  LD  $@"
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)

# Compile assembly sources
$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "  AS  $<"
	$(AS) $(ASFLAGS) -o $@ $<

# Compile C sources
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	$(CC) $(CFLAGS) -c -o $@ $<

# Create bootable ISO with GRUB
iso: $(KERNEL)
	@echo "  Creating ISO..."
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
	@cat > $(GRUB_CFG) << 'EOF'
set timeout=3
set default=0

menuentry "NovaeOS 1.0.0" {
    multiboot /boot/kernel.bin
    boot
}
EOF
	grub-mkrescue -o $(ISO) $(ISO_DIR)
	@echo "  ✓ ISO created: $(ISO)"

# Run in QEMU (no ISO needed, use kernel directly via multiboot)
run: $(KERNEL)
	@echo "  Launching NovaeOS in QEMU..."
	qemu-system-i386                    \
	    -kernel $(KERNEL)               \
	    -m 128M                         \
	    -no-shutdown                    \
	    -no-reboot                      \
	    -vga std                        \
	    -name "NovaeOS"

# Run with ISO (more realistic boot)
run-iso: $(ISO)
	qemu-system-i386                    \
	    -cdrom $(ISO)                   \
	    -m 128M                         \
	    -no-shutdown                    \
	    -no-reboot                      \
	    -vga std                        \
	    -name "NovaeOS"

# Debug: run with GDB server
run-debug: $(KERNEL)
	qemu-system-i386                    \
	    -kernel $(KERNEL)               \
	    -m 128M                         \
	    -s -S                           \
	    -vga std                        \
	    -name "NovaeOS Debug"

# Clean build artifacts
clean:
	@rm -rf $(BUILD) $(ISO_DIR) $(ISO)
	@echo "  Cleaned."

# Show build info
info:
	@echo "CC:  $(CC)"
	@echo "LD:  $(LD)"
	@echo "AS:  $(AS)"
	@echo "Sources: $(words $(C_SRCS)) C files, $(words $(ASM_SRCS)) ASM files"
