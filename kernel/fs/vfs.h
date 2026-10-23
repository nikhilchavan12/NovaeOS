/* =============================================================================
 * NovaeOS - Virtual File System (kernel/fs/vfs.h)
 * Simple in-memory VFS with a flat directory structure
 * ============================================================================= */

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define VFS_MAX_FILENAME  64
#define VFS_MAX_FILES     64
#define VFS_MAX_FILE_SIZE 4096

typedef enum {
    VFS_TYPE_FILE = 0,
    VFS_TYPE_DIR  = 1,
} vfs_type_t;

typedef struct {
    char        name[VFS_MAX_FILENAME];
    vfs_type_t  type;
    uint8_t     data[VFS_MAX_FILE_SIZE];
    size_t      size;
    bool        used;
    uint32_t    created_ticks;
    uint32_t    modified_ticks;
} vfs_node_t;

/* VFS operations */
void vfs_init(void);
int  vfs_create(const char *name, vfs_type_t type);
int  vfs_write(const char *name, const char *data, size_t len);
int  vfs_read(const char *name, char *buf, size_t buf_len);
int  vfs_delete(const char *name);
bool vfs_exists(const char *name);
int  vfs_list(char names[][VFS_MAX_FILENAME], vfs_type_t types[], int *count);
int  vfs_append(const char *name, const char *data, size_t len);
size_t vfs_get_size(const char *name);
int  vfs_rename(const char *old_name, const char *new_name);

#endif /* VFS_H */
