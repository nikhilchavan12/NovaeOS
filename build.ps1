# NovaeOS Build Script for Windows (PowerShell)
# Uses Docker to build the OS without needing local Linux tools
# Usage: .\build.ps1

param(
    [string]$Target = "run"
)

$ErrorActionPreference = "Stop"
$ProjectDir = $PSScriptRoot

Write-Host ""
Write-Host "╔══════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║       NovaeOS Windows Build Script       ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check if Docker is available
$dockerAvailable = $null -ne (Get-Command docker -ErrorAction SilentlyContinue)

if (-not $dockerAvailable) {
    Write-Host "[ERROR] Docker not found!" -ForegroundColor Red
    Write-Host "Please install Docker Desktop from: https://www.docker.com/products/docker-desktop" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Alternative: Use WSL2 (Windows Subsystem for Linux):" -ForegroundColor Yellow
    Write-Host "  1. Install WSL2: wsl --install" -ForegroundColor White
    Write-Host "  2. Open Ubuntu terminal and run:" -ForegroundColor White
    Write-Host "     sudo apt install gcc nasm binutils make qemu-system-x86" -ForegroundColor Green
    Write-Host "     cd /mnt/d/OS && make run" -ForegroundColor Green
    exit 1
}

Write-Host "[✓] Docker detected" -ForegroundColor Green

# Build the Docker image
Write-Host "[1/3] Building Docker image..." -ForegroundColor Yellow
docker build -t novaeos-builder "$ProjectDir"

# Compile the OS inside Docker
Write-Host "[2/3] Compiling NovaeOS..." -ForegroundColor Yellow
docker run --rm `
    -v "${ProjectDir}:/os" `
    novaeos-builder `
    bash -c "cd /os && make clean && make all CC=gcc LD=ld CFLAGS='-m32 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -O2 -Wall -Wextra -nostdlib -std=c11 -Ikernel' LDFLAGS='-m elf_i386 -T linker.ld'"

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "[✓] Kernel built: build/kernel.bin" -ForegroundColor Green

# Launch in QEMU
$qemuAvailable = $null -ne (Get-Command qemu-system-i386 -ErrorAction SilentlyContinue)

if ($Target -eq "run") {
    if ($qemuAvailable) {
        Write-Host "[3/3] Launching NovaeOS in QEMU..." -ForegroundColor Yellow
        qemu-system-i386 `
            -kernel "$ProjectDir\build\kernel.bin" `
            -m 128M `
            -no-shutdown `
            -no-reboot `
            -vga std `
            -name "NovaeOS"
    } else {
        Write-Host "[3/3] QEMU not found. Install from: https://www.qemu.org/download/#windows" -ForegroundColor Yellow
        Write-Host "Then run: qemu-system-i386 -kernel build\kernel.bin -m 128M" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "[✓] Done!" -ForegroundColor Green
