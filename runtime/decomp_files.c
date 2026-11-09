/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Read-only Linux bindings for the original game's five file-handle slots.
 * DOS paths are resolved under an explicit data root; guest pointers never
 * become native pointers, and no original game asset can be modified here. */

#define _POSIX_C_SOURCE 200809L

#include "decomp_files.h"
#include "decomp_image.h"
#include "ef_abi.h"
#include "ef_mem.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { MAX_DOS_PATH = 512, FILE_SLOTS = 5 };

static int root_fd = -1;
static FILE *streams[FILE_SLOTS];

static void invalid_file_service(const char *name)
{
    fprintf(stderr, "ef2000-decomp: unexpected guest file service: %s\n", name);
    abort();
}

void ef_decomp_files_end(void)
{
    unsigned int index;
    for (index = 0; index < FILE_SLOTS; index++) {
        if (streams[index])
            fclose(streams[index]);
        streams[index] = NULL;
    }
    if (root_fd >= 0)
        close(root_fd);
    root_fd = -1;
}

int ef_decomp_files_begin(const char *directory)
{
    ef_decomp_files_end();
    if (!directory)
        return 0;
    root_fd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    return root_fd >= 0 ? 0 : -1;
}

int ef_decomp_files_enabled(void)
{
    return root_fd >= 0;
}

static int ascii_same_name(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned char x = (unsigned char)*a++;
        unsigned char y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z') x = (unsigned char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (unsigned char)(y + 32);
        if (x != y) return 0;
    }
    return *a == *b;
}

/* DOS names are case-insensitive; prefer the exact name if present. */
static int open_component(int directory, const char *name, int last)
{
    const int flags = O_RDONLY | O_CLOEXEC | O_NOFOLLOW |
                      (last ? O_NONBLOCK : O_DIRECTORY);
    DIR *entries;
    struct dirent *entry;
    int result = openat(directory, name, flags);
    if (result >= 0)
        return result;

    /* dup() shares the directory read cursor; readdir() must not advance
     * our root's cursor and make later case-folded opens mysteriously fail. */
    entries = fdopendir(openat(directory, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (!entries)
        return -1;
    while ((entry = readdir(entries)) != NULL) {
        if (ascii_same_name(name, entry->d_name)) {
            result = openat(directory, entry->d_name, flags);
            break;
        }
    }
    closedir(entries);
    return result;
}

/* Skip a DOS drive prefix, but never permit '..', alternate streams or
 * symlinks to escape the explicitly selected directory. */
static int open_dos_path(const char *path)
{
    const char *cursor = path;
    int directory, result = -1;
    if (isalpha((unsigned char)cursor[0]) && cursor[1] == ':')
        cursor += 2;
    directory = openat(root_fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0)
        return -1;

    for (;;) {
        char name[256];
        size_t length = 0;
        int last, next;
        while (*cursor == '/' || *cursor == '\\')
            cursor++;
        if (*cursor == 0)
            break;
        while (cursor[length] && cursor[length] != '/' &&
               cursor[length] != '\\') {
            unsigned char c = (unsigned char)cursor[length];
            if (c < 32 || c == ':' || length >= sizeof(name) - 1)
                goto finish;
            length++;
        }
        memcpy(name, cursor, length);
        name[length] = 0;
        if (strcmp(name, "..") == 0)
            goto finish;
        cursor += length;
        if (strcmp(name, ".") == 0)
            continue;
        last = 1;
        for (const char *p = cursor; *p; p++)
            if (*p != '/' && *p != '\\') { last = 0; break; }
        next = open_component(directory, name, last);
        if (next < 0)
            goto finish;
        close(directory);
        directory = next;
        if (last) {
            struct stat st;
            if (fstat(directory, &st) == 0 && S_ISREG(st.st_mode)) {
                result = directory;
                directory = -1;
            }
            break;
        }
    }
finish:
    if (directory >= 0)
        close(directory);
    return result;
}

static FILE *get_stream(uint32_t id)
{
    if (id == 0 || id > FILE_SLOTS || !streams[id - 1])
        invalid_file_service("unknown stream handle");
    return streams[id - 1];
}

/* 0xbc36a: Watcom fopen wrapper, EAX pathname and EDX mode. The only
 * supported mode is 'rb' (at the game's read-mode constant 0x18d52c). */
uint32_t guest_fn_bc36a_w(uint32_t name, uint32_t mode)
{
    const char *dos_path, *access;
    size_t max_length;
    unsigned int i;
    int fd;
    FILE *stream;
    if (root_fd < 0 || mode != 0x0018d52cu)
        invalid_file_service("open without read-only data root");
    access = (const char *)ef_decomp_memory_span(mode, 3u);
    max_length = name < EF_DECOMP_RAM_SIZE ? EF_DECOMP_RAM_SIZE - name : 0u;
    if (max_length > MAX_DOS_PATH)
        max_length = MAX_DOS_PATH;
    dos_path = (const char *)ef_decomp_memory_span(name, max_length);
    if (!access || memcmp(access, "rb", 3) != 0 || !dos_path ||
        !memchr(dos_path, 0, max_length))
        invalid_file_service("invalid DOS filename or open mode");

    for (i = 0; i < FILE_SLOTS && streams[i]; i++) {}
    if (i == FILE_SLOTS)
        return 0;
    fd = open_dos_path(dos_path);
    if (fd < 0)
        return 0;
    stream = fdopen(fd, "rb");
    if (!stream) {
        close(fd);
        return 0;
    }
    streams[i] = stream;
    return i + 1u;
}

uint32_t guest_fn_f1d90(uint32_t dest, uint32_t size, uint32_t count, uint32_t stream)
{
    FILE *input = get_stream(stream);
    void *buffer;
    size_t bytes;
    if (size && count > SIZE_MAX / size)
        invalid_file_service("read multiplication overflow");
    bytes = (size_t)size * count;
    buffer = ef_decomp_memory_span(dest, bytes);
    if (!buffer)
        invalid_file_service("read outside guest memory");
    return (uint32_t)fread(buffer, size, count, input);
}

/* 0xa7a64's EAX/EDX/EBX arguments are seek offset, origin, handle. */
uint32_t guest_fn_a7a64_r(uint32_t offset, uint32_t origin, uint32_t handle)
{
    FILE *input;
    long position;
    /* a7a64 takes a game handle (100..104), not its backing stream ID. */
    if (handle < 100u || handle >= 100u + FILE_SLOTS ||
        EF_U32(0x001371a4u + (handle - 100u) * 4u) == 0)
        invalid_file_service("seek outside game file slots");
    input = get_stream(EF_U32(0x001371a4u + (handle - 100u) * 4u));
    if (origin > 2 || fseek(input, (int32_t)offset, (int)origin) != 0 ||
        (position = ftell(input)) < 0 || (unsigned long)position > UINT32_MAX)
        return UINT32_MAX;
    return (uint32_t)position;
}

void guest_fn_bc421(uint32_t stream, uint32_t index, uint32_t unused)
{
    if (unused != 0 || index >= FILE_SLOTS || stream != index + 1u)
        invalid_file_service("close mismatch");
    (void)get_stream(stream);
    fclose(streams[index]);
    streams[index] = NULL;
}
