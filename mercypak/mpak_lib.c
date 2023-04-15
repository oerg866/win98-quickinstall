#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <stdint.h>

#include "platform.h"
#include "mpak_lib.h"

/*
 *  mercyPack - some kind of flat binary blob file packer
 *  by oerg866
 * 
 *  Doesn't compress anything, but is more like a simple version of tarballs?
 *
 *  Packs a directory tree into a single binary blob for fast unpacking
 * 
 *  Consult MERCYPAK.TXT for file format details
 * 
 *  THIS IS FULL OF BUGS, EXPLOITS, TERRIBLE PRACTICES AND WHATEVER
 *  DON'T USE IT IN A PRODUCTIVE ENVIRONMENT
 *  I WAS JUST TRYING TO SPEEDRUN WINDOWS 9X INSTALLATION
 */

/* A linked list a day keeps the iterations away */


struct FileList {
    char *path;
    FileList *next;
    char attribute;
};

struct DirList {
    char *base;
    char *path;
    DirList *next;
    char attribute;
};


static inline bool mp_isDirectory(const char *toCheck) {
    struct stat sb;
    return (stat(toCheck, &sb) == 0 && S_ISDIR(sb.st_mode));
}

static inline bool mp_isRegularFile(const char *toCheck) {
    struct stat sb;
    return (stat(toCheck, &sb) == 0 && S_ISREG(sb.st_mode));
}

static uint8_t mp_getDosAttributes(const char *path) {
    struct stat path_stat;
    uint8_t flags = 0;
    mode_t mode;
    uint32_t winFileAttributes;

    if (stat(path, &path_stat) != 0)
        return 0x00;

    mode = path_stat.st_mode;


    if (S_ISDIR(mode))
        flags |= _A_SUBDIR;

#ifdef _WIN32
    winFileAttributes = GetFileAttributesA(path);

    if (winFileAttributes & FILE_ATTRIBUTE_HIDDEN)
        flags |= _A_HIDDEN;

    if (winFileAttributes & FILE_ATTRIBUTE_SYSTEM)
        flags |= _A_HIDDEN;
#else
    UNUSED(winFileAttributes);

    if (mode & S_ISGID)         // On FAT32 this means system file, according to some usenet posts
        flags |= _A_SYSTEM;
    
    if (mode & S_ISVTX)
        flags |= _A_HIDDEN;
#endif 

    if ((mode & (S_IREAD | S_IWRITE)) == S_IREAD)
        flags |= _A_RDONLY;

    return flags;
}

/* allocates and creates a new subpath string */
static char *mp_createSubpathString(const char* path, const char* subdir) 
{
    int pathLen = strlen(path);
    int subdirLen = strlen(subdir);
    char *ret = (char*) calloc(pathLen + subdirLen + 1 + 1, 1);
    assert(ret);

    sprintf(ret, "%s%c%s", path, PATH_SEPARATOR, subdir);
    return ret;
}

/* Adds a new directory to the linked DirList */
static DirList *mp_DirList_addNewDirectory(char *pathname, DirList *dirs, uint32_t *dirCount) 
{
    uint8_t attribute = mp_getDosAttributes(pathname);
    DirList *newDir = (DirList*) calloc(sizeof(DirList), 1);
    assert(newDir);
    newDir->base = dirs->base;
    newDir->path = pathname;
    newDir->attribute = attribute;
    if (dirs)   // Only set "next" if dir isn't null, which it is, in case it is the first directory
        dirs->next = newDir;
    *dirCount += 1;
    return newDir;
}

/* Adds a new file to the linked FileList */
static FileList *mp_FileList_addNewFile(char *filename, FileList *files, uint32_t *fileCount) 
{
    uint8_t attribute = mp_getDosAttributes(filename);
    FileList *newFile = (FileList*) calloc(sizeof(FileList), 1);
    assert(newFile);    
    newFile->path = filename;
    newFile->attribute = attribute;
    files->next = newFile;
    *fileCount += 1;
    return newFile;
}

/* Takes a host platform path string and converts it to MERCYPAK format (where path separator is \ because DOS/WIN9x).
   MUST BE free'd AFTERWARDS!! */
static char *mp_convertToMercyPakPath(const char *path) {
    char *newStr = strdup(path);
    int pathLen = strlen(path);
    int i;

    assert(newStr);
    for (i=0; i < pathLen; i++)
        if (newStr[i] == '\\' || newStr[i] == '/')
            newStr[i] = '\\';
    return newStr;
}


/* Scans a single path and adds all files in it to the linked FileList */
FileList *mp_FileList_addFilesfromDirectory(const char *path, FileList *files, uint32_t *fileCount) 
{
    FileList *ret = files;
    DIR *dirp = opendir(path);
    struct dirent *direntp;

    while(1) {
        direntp = readdir(dirp);
        if (direntp == NULL) break;

        char *tmpPath = mp_createSubpathString(path, direntp->d_name);

        if (mp_isRegularFile(tmpPath)) {            
           ret = mp_FileList_addNewFile(tmpPath, ret, fileCount);
        } else {
            free(tmpPath);
        }
        
    } 

    closedir(dirp);
    return ret;
}

/* Scans all paths in the linked DirList and adds their paths to the linked FileList */
FileList *mp_fileList_addFilesFromDirList(FileList *files, DirList *dirs, uint32_t *fileCount) 
{
    // First one is the base path
    files = mp_FileList_addFilesfromDirectory(dirs->base, files, fileCount);
    dirs = dirs->next;

    while (dirs) {
        if (dirs->path)
            files = mp_FileList_addFilesfromDirectory(dirs->path, files, fileCount);
        dirs = dirs->next;
    }
    return files;
}

/* Recursively scans a path and adds all found subdirectories to the given destination DirList. 
   Returns the new DirList tail  after scanning and sets the "next" pointer of dst to the next entry. */
static DirList *mp_MercyPackContext_findAllDirectoriesRecursive(DirList *dir, const char* path, uint32_t *dirCount) 
{  
    DirList *newDir = dir;
    DIR *dirp = opendir(path);
    struct dirent *direntp;

    assert(dirp);

    while(1) {
        direntp = readdir(dirp);
        if (direntp == NULL) break;

        char *tmpPath = mp_createSubpathString(path, direntp->d_name);

        /* If this is a proper directory, the temporary string stays allocated and taken by the DirList entry, else it gets freed. */
        if (mp_isDirectory(tmpPath) && strcmp(direntp->d_name, ".") != 0 && strcmp(direntp->d_name, "..") != 0) {
            newDir = mp_DirList_addNewDirectory(tmpPath, newDir, dirCount);
            newDir = mp_MercyPackContext_findAllDirectoriesRecursive(newDir, newDir->path, dirCount);
        } else {
            free (tmpPath);
        }
    } 

    closedir(dirp);
    return newDir;
}

/* Recursively scans a path and adds all directory and file paths to the mercypack context's list.*/
bool mp_MercyPackContext_addPath(MercyPackContext *ctx, const char* path) 
{ 
    DirList *newDirTail;
    DirList *oldDirTail;
    FileList *newFileTail;
 
     /* If DirList or FileList are not initialized, do so. */

    if (!ctx->dirs) {
        ctx->dirs = (DirList *) calloc(1, sizeof(DirList));
        assert(ctx->dirs);
        ctx->dirs->base = strdup(path);
        ctx->dirs->path = strdup(path);
        ctx->dirs->next = NULL;
        ctx->dirsTail = ctx->dirs;
    }

    if (!ctx->files) {
        ctx->files = (FileList *) calloc(1, sizeof(FileList));
        assert(ctx->files);
        ctx->filesTail = ctx->files;
    }

    /* First all directories are scanned, then all the files from all directories are scanned. Makes life a bit easier. */

    newDirTail = mp_MercyPackContext_findAllDirectoriesRecursive(ctx->dirsTail, path, &ctx->dirCount);
    oldDirTail = ctx->dirsTail;
    assert (oldDirTail && newDirTail);

    ctx->dirsTail = newDirTail;

    newFileTail = mp_fileList_addFilesFromDirList(ctx->files, oldDirTail, &ctx->fileCount);
    ctx->filesTail = newFileTail;
    return (newFileTail != NULL);
}

/* Writes file and directory count to MERCYPAK file */
static bool mp_MercyPackContext_writeDirAndFileCount(MercyPackContext *ctx) 
{
    assert(ctx->outputFile);
    return fwrite(&ctx->dirCount, sizeof(ctx->dirCount), 1, ctx->outputFile) > 0
        && fwrite(&ctx->fileCount, sizeof(ctx->fileCount), 1, ctx->outputFile) > 0;
}

/* Writes a mercypak formatted string to file */
static bool mp_MercyPackContext_writeString(MercyPackContext *ctx, const char* stringToWrite) {
    int length = strlen(stringToWrite);
    assert(ctx->outputFile);
    assert((length <= UINT8_MAX) && "String Too Long!");

    /* printf("%s length %d", stringToWrite, length & 0xff); */
    
    return fputc(length & 0xFF, ctx->outputFile) != EOF
        && fputs(stringToWrite, ctx->outputFile) >= 0;
}

/* Hopefully platform independent way of getting the DOS date out of an open file... */

static long mp_getUTCDifference() {
    static bool calculated = false;
    static long offset = 0;

    if (!calculated) {
        time_t now = time(NULL);
        struct tm local_time = *localtime(&now);
        struct tm utc_time = *gmtime(&now);

        offset = difftime(mktime(&local_time), mktime(&utc_time));
        if (local_time.tm_isdst) {
            offset += 3600;
        }
        calculated = true;
    }
    return offset;
}

static void mp_getDosDateFromFile(FILE *file, uint16_t *dosdate, uint16_t *dostime) {
    struct stat statBuf;
    int result = fstat(fileno(file), &statBuf);
    time_t dsttime = statBuf.st_mtime;
    struct tm *dst_tm;
    assert(result == 0);

    dst_tm = gmtime(&dsttime);
  
#if !defined(_WIN32)

    /* on windows there seems to be an extra offset here if DST is active... wth? */

    if (dst_tm->tm_isdst > 0) {
        dsttime -= 3600;
    }
    
#endif

    /* If host has DST, the time is interpreted differently, too. */
    /* printf("dst_tm->tm_isdst == %d, hosttime = %ld, utctime = %ld, dsttime %ld, getutcdifference= %ld\n", dst_tm->tm_isdst, hostTime, utcTime, dsttime, getUTCDifference()); */

    dsttime += mp_getUTCDifference();

    dst_tm = gmtime(&dsttime);

    *dosdate = ((dst_tm->tm_year - 80) << 9) | ((dst_tm->tm_mon + 1) << 5) | dst_tm->tm_mday;
    *dostime = (dst_tm->tm_hour << 11) | (dst_tm->tm_min << 5) | (dst_tm->tm_sec >> 1);
}

/* Write the file data block to the MercyPak output file */
static bool mp_MercyPackContext_writeFileData(MercyPackContext *ctx) {
    FileList *curFile;
    size_t basePathLen;
    FILE *toPack;
    uint16_t fileDate;
    uint16_t fileTime;
    uint32_t fileSize;
    char *toPackBuf;
    char *mpFileName;
    bool success = true;

    assert(ctx && ctx->files && ctx->dirs);

    curFile = ctx->files->next; // First entry is always blank!
    basePathLen = strlen(ctx->dirs->base);

    if (!curFile) {
        printf("No files to write\n");
        return false;
    }

    printf("file %p, files %p, dircount %d\n", ctx->outputFile, ctx->files, ctx->fileCount);

    while (curFile) {
        // First, make sure path separators written to the MP file are correct by spawning a new guaranteed mp string
        mpFileName = mp_convertToMercyPakPath(curFile->path);
        mp_MercyPackContext_writeString(ctx, &mpFileName[basePathLen+1]);
        free(mpFileName);

        fputc(curFile->attribute, ctx->outputFile);

        toPack = fopen(curFile->path, "rb");
        
        if (ferror(toPack))
            assert(0);

        fseek(toPack, 0L, SEEK_END);
        fileSize = (uint32_t) ftell(toPack);
        rewind(toPack);
        
        mp_getDosDateFromFile(toPack, &fileDate, &fileTime);
        
        printf("%s %02x %u date: %04x time: %04x\n", curFile->path, (int) curFile->attribute, (unsigned int) fileSize, fileDate, fileTime);

        fileDate = (uint16_t) fileDate;
        fileTime = (uint16_t) fileTime;

        fwrite(&fileDate, sizeof(fileDate), 1, ctx->outputFile);
        fwrite(&fileTime, sizeof(fileTime), 1, ctx->outputFile);
        fwrite(&fileSize, sizeof(fileSize), 1, ctx->outputFile);

        success &= (ferror(ctx->outputFile) || ferror(toPack));

        if (fileSize) {
            /* you run this on a modern powerful machine, it's gonna be fiiiine lmao */

            toPackBuf = (char*) malloc(fileSize);
            assert(toPackBuf);
            fread(toPackBuf, fileSize, 1, toPack);

            /* for (i = 0; i < 32; ++i )
               printf("%02x ", (int)toPackBuf[i]); 
               
            printf("\n");
            */


            fwrite(toPackBuf, fileSize, 1, ctx->outputFile);
            free(toPackBuf);
        }

        fclose(toPack);
        
        curFile = curFile->next;
    }

    return success;
}

static void mp_FileList_destroy(FileList *files) {
    FileList *curFile;
    FileList *orig;

    if (!files) return;

    curFile = files->next;
    while (curFile) {
        orig = curFile;
        curFile = curFile->next;
        free(orig->path);
        free(orig);
    }

    free(files);
}

/* Writes MERCYPAK Directory data info block */
static bool mp_MercyPackContext_writeDirectoryData(MercyPackContext *ctx) {
    DirList *curDir;
    size_t basePathLen;
    char *mpFileName;
    bool success = true;

    assert(ctx && ctx->dirs && ctx->outputFile);
    basePathLen = strlen(ctx->dirs->base);
    curDir = ctx->dirs->next; /* first entry is empty since it's the base path; */

    /* printf("file %p, dirs %p, dircount %d\n", ctx->outputFile, ctx->dirs, ctx->dirCount); */

    while (curDir) {
        mpFileName = mp_convertToMercyPakPath(&curDir->path[basePathLen+1]);
        success &= (fputc(curDir->attribute, ctx->outputFile) != EOF);
        success &= mp_MercyPackContext_writeString(ctx, mpFileName);
        curDir = curDir->next;
        free(mpFileName);
    }

    return success;
}

/* Writes MERCYPAK magic number to file */
static bool mp_MercyPackContext_writeFileHeader(MercyPackContext *ctx) 
{
    return (fprintf(ctx->outputFile, MERCYPAK_MAGIC_NUMBER) > 0);
}

/* Traverses and Destroys an entire DirList*/
static void mp_DirList_destroy(DirList *dirs) {
    DirList *curDir;
    DirList *orig;

    if (!dirs) return;

    curDir = dirs->next; /* first entry is empty since it's the base path */

    /* The base path is the same for all the entries in a DirList. Hence, we only free it once. */

    free(dirs->base);

    while (curDir) {
        orig = curDir;
        curDir = curDir->next;
        free(orig->path);
        free(orig);
    }

    free(dirs);
}

/* Pack all files added to the MercyPackContext and write the output file. */
bool mp_MercyPackContext_doPackAll(MercyPackContext *ctx) {

    if(!ctx->outputFile)
        ctx->outputFile = fopen(ctx->outputFileName, "wb");

    if (!ctx->outputFile) return false;
    if (!mp_MercyPackContext_writeFileHeader(ctx)) return false;
    if (!mp_MercyPackContext_writeDirAndFileCount(ctx)) return false;
    if (!mp_MercyPackContext_writeDirectoryData(ctx)) return false;
    if (!mp_MercyPackContext_writeFileData(ctx)) return false;

    fclose(ctx->outputFile);
    return true;
}

MercyPackContext *mp_MercyPackContext_init(char *targetFileName) {
    MercyPackContext *ctx = (MercyPackContext *) calloc(1, sizeof(MercyPackContext));
    if (!ctx) return NULL;

    ctx->outputFileName=strdup(targetFileName);
    return ctx;
}

void mp_MercyPackContext_destroy(MercyPackContext *ctx) {
    if (ctx) {
        if (ctx->outputFile) fclose(ctx->outputFile);
        mp_DirList_destroy(ctx->dirs);
        mp_FileList_destroy(ctx->files);
        free(ctx->outputFileName);
        free(ctx);
    }    
}