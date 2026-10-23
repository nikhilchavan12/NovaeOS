/* =============================================================================
 * NovaeOS - VFS Implementation (kernel/fs/vfs.c)
 * In-memory flat filesystem (no disk I/O needed)
 * ============================================================================= */

#include "vfs.h"
#include "../drivers/timer.h"
#include "../mm/heap.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Simple string functions (no libc) */
static size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}
static int kstrcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
static void kstrcpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static vfs_node_t vfs_table[VFS_MAX_FILES];
static int vfs_file_count = 0;

static vfs_node_t *find_node(const char *name) {
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (vfs_table[i].used && kstrcmp(vfs_table[i].name, name) == 0)
            return &vfs_table[i];
    }
    return NULL;
}

static vfs_node_t *find_free_slot(void) {
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!vfs_table[i].used) return &vfs_table[i];
    }
    return NULL;
}

void vfs_init(void) {
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        vfs_table[i].used = false;
        vfs_table[i].size = 0;
    }
    vfs_file_count = 0;

    /* Create some default files */
    vfs_create("README.txt", VFS_TYPE_FILE);
    vfs_write("README.txt",
        "Welcome to NovaeOS!\n"
        "===================\n\n"
        "NovaeOS is a 32-bit x86 operating system kernel.\n\n"
        "Built with: GCC, NASM, GNU ld\n"
        "Runs on:    QEMU, Bochs, or real hardware\n\n"
        "Type 'help' in the shell to see available commands.\n",
        0);

    vfs_create("hello.txt", VFS_TYPE_FILE);
    vfs_write("hello.txt", "Hello from NovaeOS!\nThis is your first file.\n", 0);

    vfs_create("motd.txt", VFS_TYPE_FILE);
    vfs_write("motd.txt",
        "Message of the Day:\n"
        "  'An operating system is the foundation of all software.' \n",
        0);
}

int vfs_create(const char *name, vfs_type_t type) {
    if (find_node(name)) return -1;  /* Already exists */
    vfs_node_t *node = find_free_slot();
    if (!node) return -2;            /* No space */
    kstrcpy(node->name, name, VFS_MAX_FILENAME);
    node->type           = type;
    node->size           = 0;
    node->used           = true;
    node->created_ticks  = timer_get_ticks();
    node->modified_ticks = node->created_ticks;
    vfs_file_count++;
    return 0;
}

int vfs_write(const char *name, const char *data, size_t len) {
    vfs_node_t *node = find_node(name);
    if (!node) return -1;
    if (len == 0) len = kstrlen(data);
    if (len > VFS_MAX_FILE_SIZE) len = VFS_MAX_FILE_SIZE;
    kmemcpy(node->data, data, len);
    node->size           = len;
    node->modified_ticks = timer_get_ticks();
    return (int)len;
}

int vfs_append(const char *name, const char *data, size_t len) {
    vfs_node_t *node = find_node(name);
    if (!node) return -1;
    if (len == 0) len = kstrlen(data);
    size_t remaining = VFS_MAX_FILE_SIZE - node->size;
    if (len > remaining) len = remaining;
    kmemcpy(node->data + node->size, data, len);
    node->size          += len;
    node->modified_ticks = timer_get_ticks();
    return (int)len;
}

int vfs_read(const char *name, char *buf, size_t buf_len) {
    vfs_node_t *node = find_node(name);
    if (!node) return -1;
    size_t to_copy = node->size < buf_len - 1 ? node->size : buf_len - 1;
    kmemcpy(buf, node->data, to_copy);
    buf[to_copy] = '\0';
    return (int)to_copy;
}

int vfs_delete(const char *name) {
    vfs_node_t *node = find_node(name);
    if (!node) return -1;
    node->used = false;
    node->size = 0;
    vfs_file_count--;
    return 0;
}

bool vfs_exists(const char *name) {
    return find_node(name) != NULL;
}

int vfs_list(char names[][VFS_MAX_FILENAME], vfs_type_t types[], int *count) {
    int n = 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (vfs_table[i].used) {
            kstrcpy(names[n], vfs_table[i].name, VFS_MAX_FILENAME);
            types[n] = vfs_table[i].type;
            n++;
        }
    }
    *count = n;
    return 0;
}

size_t vfs_get_size(const char *name) {
    vfs_node_t *node = find_node(name);
    return node ? node->size : 0;
}

int vfs_rename(const char *old_name, const char *new_name) {
    vfs_node_t *node = find_node(old_name);
    if (!node) return -1;
    if (find_node(new_name)) return -2;  /* New name exists */
    kstrcpy(node->name, new_name, VFS_MAX_FILENAME);
    return 0;
}
