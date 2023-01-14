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

static bool mp_isDirectory(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

static bool mp_isRegularFile(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

static char *mp_createSubpathString(const char* path, const char* subdir) 
/* allocates and creates a new subpath string */
{
    int pathLen = strlen(path);
    int subdirLen = strlen(subdir);
    char *ret = (char*) calloc(pathLen + subdirLen + 1 + 1, 1);
    assert(ret);

    sprintf(ret, "%s%c%s", path, PATH_SEPARATOR, subdir);
    return ret;
}

static DirList *mp_DirList_addNewDirectory(const char *path, const char* subdir, uint8_t attribute, DirList *dirs, uint32_t *dirCount) 
/* Adds a new directory to the linked DirList */
{
    DirList *newDir = (DirList*) calloc(sizeof(DirList), 1);
    assert(newDir);
    newDir->base = dirs->base;
    newDir->path = mp_createSubpathString(path, subdir);
    newDir->attribute = attribute;
    if (dirs)   // Only set "next" if dir isn't null, which it is, in case it is the first directory
        dirs->next = newDir;
    *dirCount += 1;
    //printf("%d: %s\n", *dirCount, newDir->path);
    return newDir;
}

static FileList *mp_FileList_addNewFile(const char *path, const char* filename, uint8_t attribute, FileList *files, uint32_t *fileCount) 
/* Adds a new file to the linked FileList */
{
    FileList *newFile = (FileList*) calloc(sizeof(FileList), 1);
    newFile->path = mp_createSubpathString(path, filename);
    newFile->attribute = attribute;
    files->next = newFile;
    *fileCount += 1;
    return newFile;
}

/* Takes a MERCYPAK path string and converts it to a string the host platform can understand. MUST BE free'd AFTERWARDS!! 
   Important for DOS and stuff... */
static char *mp_convertToHostPath(const char *path) {
    char *newStr = strdup(path);
    int pathLen = strlen(path);
    int i;

    assert(newStr);
    for (i=0; i < pathLen; i++)
        if (newStr[i] == '\\' || newStr[i] == '/')
            newStr[i] = PATH_SEPARATOR;
    return newStr;
}

static uint8_t mp_getDosAttributesFromDirent(struct dirent *direntp) {
    struct stat path_stat;
    uint8_t flags = 0;
    mode_t mode;
    uint32_t winFileAttributes;

    assert(direntp);

    if (!direntp || (stat(direntp->d_name, &path_stat) != 0))
        return 0x00;

    mode = path_stat.st_mode;


    if (S_ISDIR(mode))
        flags |= _A_SUBDIR;

#ifdef _WIN32
    winFileAttributes = GetFileAttributesA(direntp->d_name);

    if (winFileAttributes & FILE_ATTRIBUTE_HIDDEN)
        flags |= _A_HIDDEN;

    if (winFileAttributes & FILE_ATTRIBUTE_SYSTEM)
        flags |= _A_HIDDEN;
#else
    UNUSED(winFileAttributes);

    if (mode & S_ISGID)         // On FAT32 this means system file
        flags |= _A_SYSTEM;
    
    if (mode & S_ISVTX)
        flags |= _A_HIDDEN;
#endif 

    if ((mode & (S_IREAD | S_IWRITE)) == S_IREAD)
        flags |= _A_RDONLY;

    return flags;
}

FileList *mp_FileList_addFilesfromDirectory(const char *path, FileList *files, uint32_t *fileCount) 
/* Scans a single path and adds all files in it to the linked FileList */
{
    FileList *ret = files;
    DIR *dirp = opendir(path);
    struct dirent *direntp;

    while(1) {
        direntp = readdir(dirp);
        if (direntp == NULL) break;

        if (mp_isRegularFile(direntp->d_name)) {
            ret = mp_FileList_addNewFile(path, direntp->d_name, mp_getDosAttributesFromDirent(direntp), ret, fileCount);
        }
    } 

    closedir(dirp);
    return ret;
}

FileList *mp_fileList_addFilesFromDirList(FileList *files, DirList *dirs, uint32_t *fileCount) 
/* Scans all paths in the linked DirList and adds their paths to the linked FileList */
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
    uint8_t attributeFlags;

    assert(dirp);

    while(1) {
        direntp = readdir(dirp);
        if (direntp == NULL) break;

        if (mp_isDirectory(direntp->d_name) && strcmp(direntp->d_name, ".") != 0 && strcmp(direntp->d_name, "..") != 0) {
            attributeFlags = mp_getDosAttributesFromDirent(direntp);
            newDir = mp_DirList_addNewDirectory(path, direntp->d_name, attributeFlags, newDir, dirCount);
            newDir = mp_MercyPackContext_findAllDirectoriesRecursive(newDir, newDir->path, dirCount);
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
 
     /* If DirList is not initialized, do so. */

    if (!ctx->dirs) {
        ctx->dirs = (DirList *) calloc(1, sizeof(DirList));
        assert(ctx->dirs);
        ctx->dirs->base = strdup(path);
        ctx->dirs->path = strdup(path);
        ctx->dirs->next = NULL;
        ctx->dirsTail = ctx->dirs;
        // Add all files from base path first
    }

    if (!ctx->files) {
        ctx->files = (FileList *) calloc(1, sizeof(FileList));
        assert(ctx->files);
        ctx->filesTail = ctx->files;
    }

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
static void mp_getDosDateFromFile(FILE *file, uint16_t *date, uint16_t *time) {
    struct stat statBuf;
    int result = fstat(fileno(file), &statBuf);
    struct tm *tm_struct;
    assert(result == 0);

    tm_struct = gmtime(&statBuf.st_mtime);

    *date = ((tm_struct->tm_year - 80) << 9) | ((tm_struct->tm_mon + 1) << 5) | tm_struct->tm_mday;
    *time = (tm_struct->tm_hour << 11) | (tm_struct->tm_min << 5) | (tm_struct->tm_sec >> 1);
}

/* Write the file data block to the MercyPak output file */
static bool mp_MercyPackContext_writeFileData(MercyPackContext *ctx) { //FILE *file, FileList *files, DirList *dirs, size_t fileCount) {
    FileList *curFile;
    FileList *orig;
    size_t basePathLen;
    FILE *toPack;
    uint16_t fileDate;
    uint16_t fileTime;
    uint32_t fileSize;
    char *toPackBuf;
    char *hostFileName;
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
        mp_MercyPackContext_writeString(ctx, &curFile->path[basePathLen+1]);
        fputc(curFile->attribute, ctx->outputFile);

        hostFileName = mp_convertToHostPath(curFile->path);

        toPack = fopen(hostFileName, "rb");
        
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
        
        orig = curFile;
        curFile = curFile->next;
        free(orig->path);
        free(orig);
        free(hostFileName);
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
}

/* Writes MERCYPAK Directory data info block */
static bool mp_MercyPackContext_writeDirectoryData(MercyPackContext *ctx) {
    DirList *curDir;
    size_t basePathLen;
    char *hostFileName;
    bool success = true;

    assert(ctx && ctx->dirs && ctx->outputFile);
    basePathLen = strlen(ctx->dirs->base);
    curDir = ctx->dirs->next; /* first entry is empty since it's the base path; */

    /* printf("file %p, dirs %p, dircount %d\n", ctx->outputFile, ctx->dirs, ctx->dirCount); */

    while (curDir) {
        hostFileName = mp_convertToHostPath(&curDir->path[basePathLen+1]);
        success &= (fputc(curDir->attribute, ctx->outputFile) != EOF);
        success &= mp_MercyPackContext_writeString(ctx, hostFileName);
        curDir = curDir->next;
        free(hostFileName);
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