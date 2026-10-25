# =============================================================================
# NovaeOS - Dockerfile
# Multi-stage build: compile the OS in a Linux container with all tools
# =============================================================================
# Usage:
#   docker build -t novaeos-builder .
#   docker run --rm -v "%cd%":/os novaeos-builder
#   Then run: qemu-system-i386 -kernel build/kernel.bin -m 128M
# =============================================================================

FROM ubuntu:22.04 AS builder

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential     \
    nasm                \
    binutils            \
    gcc                 \
    make                \
    grub-pc-bin         \
    grub-common         \
    xorriso             \
    # Cross-compiler for i686-elf (optional, falls back to system gcc)
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /os

# Copy all source files
COPY . .

# Build using system gcc (32-bit mode) since we may not have cross-compiler
# We override the CC/LD to use system tools with 32-bit flags
RUN make all


# Output: /os/build/kernel.bin
CMD ["bash", "-c", "echo 'Build done. Kernel at: /os/build/kernel.bin'"]
