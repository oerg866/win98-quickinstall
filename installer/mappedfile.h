#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>

typedef struct {
    int fd;
    size_t size;
    size_t pos;
    uint8_t *mem;
} mappedFile;

mappedFile *mappedFile_open(const char *filename, size_t readahead);
void mappedFile_close(mappedFile *file);
bool mappedFile_copyToFile(mappedFile *file, int outfd, size_t size);
bool mappedFile_read(mappedFile *file, void *dst, size_t len);

#define mappedFile_eof(file)            (file->pos >= file->size)
#define mappedFile_available(file)      (file->size - file->pos)
#define mappedFile_getUInt8(file, dst)  (mappedFile_read(file, dst, sizeof(uint8_t)))
#define mappedFile_getUInt16(file, dst) (mappedFile_read(file, dst, sizeof(uint16_t)))
#define mappedFile_getUInt32(file, dst) (mappedFile_read(file, dst, sizeof(uint32_t)))


