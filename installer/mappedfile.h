/*
 * LUNMERCY
 * Mapped File Reader
 *
 * Mapped Files are.
 * 
 * The name comes from the initial implementation which uses MMAP.
 * Implementations available are:
 *      mappedfile_mt.c (multi threaded using raw read/write) -- EXPERIMENTAL
 *      mappedfile.c (single-threaded using mmap)
 *
 * Still trying to figure out what is the fastest way to do IO on a slow 486... :S
 *
 * (C) 2024 Eric Voirin (oerg866@googlemail.com)
 */

#ifndef _MAPPEDFILE_H_
#define _MAPPEDFILE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct MappedFile MappedFile;

// Open the mapped File. Readahead is a parameter indicating how much RAM the system can spare to read ahead.
MappedFile *mappedFile_open(const char *filename, size_t readahead);
// Closes the file and releases all resources associated with it
void        mappedFile_close(MappedFile *file);

// File read operations - these all advance the internal read position.

// Copy data from the file at the current read position to a set of open file descriptors, pointed to by fileCount and outfds.
bool        mappedFile_copyToFiles(MappedFile *file, size_t fileCount, int *outfds, size_t len);
// Reads data of arbitrary length and copies it to dst.
bool        mappedFile_read(MappedFile *file, void *dst, size_t len);
// Reads an uint8_t and copies it to dst.
bool        mappedFile_getUInt8(MappedFile *file, uint8_t *dst);
// Reads an uint16_t and copies it to dst.
bool        mappedFile_getUInt16(MappedFile *file, uint16_t *dst);
// Reads an uint32_t and copies it to dst.
bool        mappedFile_getUInt32(MappedFile *file, uint32_t *dst);

// Obtains the size of the opened file
size_t      mappedFile_getFileSize(MappedFile *file);
// Obtains the current read position of the opened file
size_t      mappedFile_getPosition(MappedFile *file);

#endif