#!/bin/python3

# Drivercopy for Windows 98 QuickInstall.
# Python version.
# (C) 2014-2026 Eric Voirin (oerg866@googlemail.com)

# DRIVERCOPY is a program written specifically for WIN98QI to recursively scan a directory full of drivers.
# It originated from my very first win32api-based C++ code back in 2014.

# Drivercopy parses the INF files in the driver libraries, checks them for validity and removes catalog file references.
# It then packs their referenced driver files in a CAB archive and copies that file into the destination folder,
# along with the modified INF file.

# History:
# 2026-04-18: Fix cab file name conflict eating up some drivers' files
# 2026-02-03: Python rewrite, remove all CatalogFile references to prevent missing file popups during HW detection
# 2025-02-19: Fix CatalogFile entries with nonexistant files causing CAB file creation to fail (fixes ArtX)
# 2024-10-21: Fix problems with non-unique INF file names causing CAB files to be overwritten and thus missing driver files.

import os
import shutil
import argparse
from datetime import datetime
from struct import unpack
from cabarchive import CabArchive, CabFile
from wininfparser import WinINF, INFsection

def msExpandDecompress(data: bytes) -> bytes:
    """
    Decompress Microsoft LZSS compressed data into raw bytes.

    Supports two formats:
      - SZDD: Standard Microsoft COMPRESS.EXE format with 14-byte header
               (magic 'SZDD', mode byte, missing char, uncompressed size)
      - Headerless: Raw LZSS payload without any header, as used by some
               OEM driver packaging tools from the Windows 3.x/95 era.

    Both formats use the same underlying LZSS algorithm with a 4096-byte
    ring buffer pre-filled with spaces. Each flag byte (LSB first) controls
    8 items: set bit = literal byte, clear bit = 2-byte backreference
    encoding a 12-bit offset and 4-bit length (+3).

    SZDD header written by Alpon Sepriando for SATAID (MIT License)
    https://pypi.org/project/sataid/
    Headerless variant added for OEM driver support.
    """
    if len(data) >= 14 and data[:4] == b"SZDD":
        # SZDD format: parse header for uncompressed size, skip 14-byte header
        out_len = unpack('I', data[10:14])[0]
        payload = data[14:]
        # SZDD uses ring position 4096 - 16 = 0xFF0
        ring_start = 4096 - 16
    else:
        # Headerless format: entire file is the LZSS payload
        out_len = 0
        payload = data
        # Headerless variant uses ring position 0xFEE = 4096 - 18
        ring_start = 0xFEE

    # LZSS sliding window (4096 bytes), initialized with spaces
    window = bytearray(4096)
    for i in range(4096):
        window[i] = 0x20  # space character

    pos = ring_start
    out = bytearray()

    p = 0
    while p < len(payload):
        control = payload[p]
        p += 1

        bit = 1
        while bit <= 0x80 and p < len(payload):
            if control & bit:
                # Literal byte
                if p >= len(payload):
                    break
                ch = payload[p]
                p += 1
                out.append(ch)
                window[pos] = ch
                pos = (pos + 1) & 0xFFF
            else:
                # Bit clear: backreference (2 bytes -> 12-bit offset + 4-bit length)
                if p + 1 > len(payload):
                    break
                matchpos = payload[p]
                matchlen = payload[p + 1]
                p += 2

                # High nibble of second byte provides top 4 bits of offset
                matchpos |= (matchlen & 0xF0) << 4
                # Low nibble of second byte is length minus 3 (min match = 3)
                matchlen = (matchlen & 0x0F) + 3

                for _ in range(matchlen):
                    c = window[matchpos & 0xFFF]
                    matchpos = (matchpos + 1) & 0xFFF
                    out.append(c)
                    window[pos] = c
                    pos = (pos + 1) & 0xFFF

            bit <<= 1

    # SZDD header specifies exact output size; trim if known
    if out_len > 0 and len(out) >= out_len:
        out = out[:out_len]

    return bytes(out)

def getDateTimeFromFile(fileName: str) -> datetime:
    timestamp = os.path.getmtime(fileName)
    return datetime.fromtimestamp(timestamp)

class SourceFile:
    _win98files = None

    @classmethod
    def _initialize(cls):
        if cls._win98files is None:
            win98filesDat = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'win98files.dat')
            with open(win98filesDat, "r") as f: cls._win98files = f.read().splitlines()
            print(f'Initialized SourceFile class with {len(cls._win98files)} known Windows 98 files.')


    def __init__(self, fileName: str, localDir: str, sourceDir: str):
        self.__class__._initialize()
        self.fileName = fileName    # The file name for this entry
        self.sourceDir = sourceDir  # The optional source directory for this entry
        self.isFromWin98 = containsCaseInsensitive(self._win98files, fileName)
        self.localName = self.getSourceFileNameSourceDiskDirectoryOrLocal(fileName, sourceDir, localDir)
        self.data = None

        # Build compressed file name in case it is a SZDD compressed file.
        base, ext = os.path.splitext(self.fileName)
        if len(ext) == 4: ext = ext[:3]
        fileNameCompressed =  base + ext + '_'        
        self.localCompressed = self.getSourceFileNameSourceDiskDirectoryOrLocal(fileNameCompressed, sourceDir, localDir)

        if self.isFromWin98 and self.localName:
            logw(f'WARNING: File {self.localName} supplied by Win9x AND driver - potential conflict!!!')

        if self.localName:
            # File is available locally, no problem.
            logd(f'Reading data from local file {self.localName}')
            self.mtime = getDateTimeFromFile(self.localName)
            self.data = open(self.localName, 'rb').read()
        elif self.localCompressed:
            logd(f'Reading data from local MSExpand compressed file {self.localCompressed}')
            self.mtime = getDateTimeFromFile(self.localCompressed)
            fileDataCompressed = open(self.localCompressed, 'rb').read()
            try:
                self.data = msExpandDecompress(fileDataCompressed)
                if len(self.data) == 0: logw(f'Decompressed file {self.localCompressed} empty?!')
                logd(f'Read {self.localCompressed} (MSExpand Decompressed {len(self.data)} Bytes)')
            except:
                loge(f'Error: file decompression failed!')

        if (self.data is None) and self.isFromWin98:
            logd(f'File {self.fileName} supplied by Win9x!')
        elif (self.data is None):
            loge(f'File {self.fileName} is NOT supplied by driver nor Win9x itself!')
            raise Exception(f'Please fix {self.fileName} before packaging ISO!')

    def getSourceFileNameSourceDiskDirectoryOrLocal(self, fileName: str, sourceDir: str, localDir: str):
        result = None
        subDir = findSubPath(localDir, sourceDir)

        logd(f'Subdir {sourceDir} {localDir} {subDir}')
        
        if subDir:
            result = getFilenameCaseInsensitive(subDir, fileName)

        if not result and subDir != localDir:
            result = getFilenameCaseInsensitive(localDir, fileName)
            if result:
                logw(f'Warnnig: file {fileName} was expected to be found in {sourceDir} but was found in {localDir}')

        return result

OK        = '\033[92m'
WARN      = '\033[93m'
ERROR     = '\033[91m'
DEBUG     = '\033[94m'
ENDC      = '\033[0m'

ENABLE_DEBUG_LOGS = False

def loge(whatToPrint):
    print(ERROR, whatToPrint, ENDC, sep="")
def logw(whatToPrint):
    print(WARN, whatToPrint, ENDC, sep="")
def logi(whatToPrint):
    print(OK, whatToPrint, ENDC, sep="")
def logd(whatToPrint):
    if ENABLE_DEBUG_LOGS:
        print(DEBUG, whatToPrint, ENDC, sep="")

# Based on <filename>, obtain an unique file name in the <path> directory
# so that two <filename> files will be able to be saved.
# A counter is applied to filename.
# This assumes that the filename is <= 8 characters long because DOS stuff
def getUniqueFilename(path: str, filename: str):
    nameNoDir = os.path.basename(filename)
    nameNoExt, ext = os.path.splitext(nameNoDir)

    fullName = os.path.join(path, nameNoDir)
    counter = 1
    while os.path.exists(fullName): 
            counterStr = f'{counter}'
            namewithCounter = nameNoExt + counterStr

            if (len(namewithCounter) > 8):
                stumpLen = 8 - len(counterStr)
                namewithCounter = nameNoExt[:stumpLen] + counterStr

            fullName = os.path.join(path, namewithCounter + ext)

            counter +=1

    return fullName

# Retreive the real spelling of a key, case insensitive,  from an INF section 
def findKey(section: INFsection, toFind: str):
    for key, value, comment in section:
        if (key.lower() == toFind.lower()):
            return key

    return None

# Get INFsection from given section name, case-insensitively
def getSection(inf: WinINF, section: str):
    toFind = section.lower().strip()
    for sec in inf:
        if sec.GetName().lower().strip() == toFind:
            return sec
    return None

# Remove the CatalogFile reference from the Version section.
def removeCatFile(versionSection: INFsection):
    catKey = findKey(versionSection, 'CatalogFile')
    if catKey:
        logw(f'Removing catalog reference: {versionSection.Find(catKey)}')
        versionSection.RemoveKey(catKey)

# Get the value for a given key in the given INF sectioni, case insensitively. Returns "" if nonexistant
def getValue(section: INFsection, toFind:str):
    for key, value, comment in section:
        if (key.lower() == toFind.lower()):
            return value
        
    return ""

# Appends a string to a string list if not present already
def appendIfNew(strList: list[str], value: str):
    if value not in strList:
        strList.append(value)

# gets a list of all CopyFiles sections. Direct file references (@<something>) are appended to rawFiles.
def getCopyFilesSections(inf: WinINF, rawFiles: list[str], localDir: str):
    sections = []

    for section in inf:

        # wininfparser is case sensitive which is bad because windows is not :P        
        cfLine = getValue(section, 'CopyFiles')

        if not cfLine:
            continue

        newSections = cfLine.split(',')

        for cfSec in newSections:
            cfSec = cfSec.strip()

            # An individual file can be copied by prefixing the file name with an @ symbol (!!!)
            # Source: http://www-pc.uni-regensburg.de/systemsw/TECHTOOL/w95/doc/INFDOC.HTM#CopyFilesSections
            if cfSec.startswith('@'):
                addOrUpdateSourceFile(rawFiles, cfSec[1:], localDir)
            else:
                appendIfNew(sections, cfSec)

    return sections

# Get a source dir override from a sourceDisksFiles list for the given filename
# Returns empty string if not found
def getSourceDisksFilesDirOverride(fileName: str, sourceDisksFiles: list[tuple[str, str]]) -> str:
    for sdf in sourceDisksFiles:
        fname, dir = sdf

        if fname.lower() == fileName.lower() and len(dir) > 0:
            return dir
        
    return ""

# Get a list of all files referenced by all CopyFiles sections in the INF
def getAllFilesFromSections(
        inf: WinINF,
        sectionNames: list[str],
        sourceDisksFiles: list[tuple[str,str]],
        knownFiles: list[SourceFile],
        localDir: str):

    sectionFiles = []
    sourceDisksFilesLeftToProcess = []

    # Construct a list of section file names

    for secName in sectionNames:
        section = getSection(inf, secName)

        if section is None:
            logw(f'Reference to nonexistant (could be from layout INF?) CopyFiles section {secName}')
            continue

        for key, value, comment in section:
            if (key):
                # CopyFiles files are CSV things
                fileFields = key.split(',')

                # We have a 2nd field AND its length is 0 -> take the source fiel name instead
                if len(fileFields) >= 2 and len(fileFields[1]) > 0:
                    fname = fileFields[1]
                else: 
                    fname = fileFields[0]

                sectionFiles.append(fname.strip())

    # Now construct a list of source disks files that weren't referenced in the sections

    for sdf in sourceDisksFiles:
        fname, dir = sdf
        if not containsCaseInsensitive(sectionFiles, fname):
            # Section files list doesn't contain this source disk file, add it to "process later list"
            sourceDisksFilesLeftToProcess.append(sdf)

    # Now process all the section files that *are* in the source disks files list

    for secf in sectionFiles:
        # Get source dir override, if it exists
        sourceDir = getSourceDisksFilesDirOverride(secf, sourceDisksFiles)
        # SourceDir will be empty if it's not found in SDF list, which gets handled by this function properly
        # so no need to distinguish
        addOrUpdateSourceFile(knownFiles, secf, localDir, sourceDir)

    # Now process all the files that weren't referenced in the sections

    for sdf in sourceDisksFilesLeftToProcess:
        fname, sourceDir = sdf
        addOrUpdateSourceFile(knownFiles, fname, localDir, sourceDir)


# Add or update an entry in a SourceFile list. Local dir = the base dir of the inf
# sourceDir = the source dir for the file as specified *IN* the inf. Sorry it's a bit confusing
def addOrUpdateSourceFile(sfList: list[SourceFile], filename: str, localDir: str, sourceDir: str=''):
    for sfEntry in sfList:
        # If we have this entry already AND the INF file matches, update its source dir
        if sfEntry.fileName.lower() == filename.lower():

            if sfEntry.sourceDir and sfEntry.sourceDir.lower() != sourceDir.lower():
                loge('Ambiguious source file directory')
                raise Exception(f'Source file {filename} source directory is not unique in this folder!')

            sfEntry.sourceDir = sourceDir
            return
    # Else, add a new one
    sfList.append(SourceFile(filename, localDir, sourceDir))

# Get the value string out of the SourceDisksNames section for a given source disk index
def getSourceDiskString(inf: WinINF, sourceDiskIndex: str):
    sdnSection = getSection(inf, 'SourceDisksNames')
    if not sdnSection:
        return None
    
    for key, value, comment in sdnSection:
        if key.lower() == sourceDiskIndex.lower():
            return value

    return None

# Get a list of all files referenced by the SourceDisksFiles section of this INF.
# Return value is a list of tuples [filename, sourceDiskDir]
def getSourceDisksFiles(inf: WinINF) -> list[tuple[str,str]]:
    ret = []
    
    sdfSection = getSection(inf, 'SourceDisksFiles')
    
    if not sdfSection:
        return ret
    
    for key, value, comment in sdfSection:
        if not key: 
            continue

        sourceDir = ""

        # Value Might contain a source disk ID
        if value:
            sourceDiskIndex = value.strip().split(',')[0]

            # If it does have one, get the associated sourceDisksNames entry
            sourceDiskString = getSourceDiskString(inf, sourceDiskIndex)

            if sourceDiskString:
                logd(f'SourceDisk {sourceDiskIndex} for file {key}: {sourceDiskString}')

                # We have a source disk string, split it by commas
                sdFields = sourceDiskString.split(',')

                if len(sdFields) >= 4:
                    dir = sdFields[3].strip()
                    if dir:
                        sourceDir = dir

            else:
                logw(f'File entry with missing Source Disk {sourceDiskIndex}')
        
        ret.append([key.strip(), sourceDir])

    return ret


# Re-write the SourceDisksNames and SourceDisksFiles sections of the inf file from scratch
# according to the info we have now after scanning
def writeSourceDisksNamesAndFiles(inf: WinINF, knownFiles: list[SourceFile], cabName: str):
    sdnSection = getSection(inf, 'SourceDisksNames')

    # Remove existing SourceDisksNames section
    if (sdnSection):
        diskCount = 0
        for key, value, comment in sdnSection:
            if len(key.strip()) > 0:
                diskCount += 1

        if diskCount > 1:
            logw(f'Inf file contains more than one SourceDisksNames entry. Output may be broken!')

        inf.RemoveSection(sdnSection)

    sdfSection = getSection(inf, 'SourceDisksFiles')

    # Remove existing SourceDisksFiles section
    if (sdfSection):
        inf.RemoveSection(sdfSection)

    cabNameNoDir = os.path.basename(cabName)

    # Before we re-add the sections, we should add a blank line at the end of the last section to make it nicer to read
    inf.Last().AddData('')

    sdnSection = INFsection()
    sdnSection.SetName('SourceDisksNames')
    sdnSection.AddData('1', f'\"Windows 98 QuickInstall Driver Pack\",{cabNameNoDir},,', '-- by Win9xQI DriverCopy --')

    sdnSection.AddData('')

    inf.AddSection(sdnSection)

    sdfSection = INFsection()
    sdfSection.SetName('SourceDisksFiles')

    for sfEntry in knownFiles:
        sdfSection.AddData(f'{sfEntry.fileName}', '1')

    sdfSection.AddData('')

    inf.AddSection(sdfSection)

# Scan an inf file, collect all its associated files, modify it according to what the CAB name will be, and save it to output
def handleInf(filename: str, outInfName: str, localDir: str, cabName: str, filesInDirectory: list[SourceFile], simulate: bool = False) -> bool:
    inf = WinINF()
    inf.ParseFile(filename, codec='cp1252')

    knownFiles = list[SourceFile]()

    # Check if this is a valid inf file
    versionSection = getSection(inf, 'Version')

    if versionSection is None:
        loge(f'File {filename} is not a valid driver INF, skipping...')
        return False

    versionSection.AddComment('----------- Windows 98 QuickInstall DriverCopy Slipstreamed INF file --------------')
    versionSection.AddComment('------------------------------ you driver me wild ---------------------------------')

    # Remove catalog file if present
    removeCatFile(versionSection)

    # All copyfiles sections must be scanned for drivers
    copyFilesSections = getCopyFilesSections(inf, knownFiles, localDir)

    # Gather all related files from this driver

    # First of all, get all references in SourceDisksFiles, to know if the source dir is being set
    sourceDisksFiles = getSourceDisksFiles(inf)
    getAllFilesFromSections(inf, copyFilesSections, sourceDisksFiles, knownFiles, localDir)


    writeSourceDisksNamesAndFiles(inf, knownFiles, cabName)

    if not simulate:
        success = inf.Save(outInfName, codec="cp1252")
    else:
        success = True

    # Wininfparser saves with \n line endings, which Win98 doesn't really like.
    # So we replace them by hand with \r\n


    for newFile in knownFiles:
        addOrUpdateSourceFile(filesInDirectory, newFile.fileName, localDir, newFile.sourceDir)

    return success

# Checks if a string list contains an item, case insensitively
def containsCaseInsensitive(where: list[str], toFind: str) -> bool:
    if toFind.lower() in (s.lower() for s in where):
        return True
    else:
        return False

# Get a correctly capitalized filename in the source directory (so a case insensitive lookup)
def getFilenameCaseInsensitive(sourceDir: str, filename: str):
    filename = filename.replace('\\', os.path.sep)

    logd(f'finding {filename} in {sourceDir}')

    if not os.path.exists(sourceDir):
        raise Exception(f"Source directory {sourceDir} doesn't exist?!")

    for f in os.listdir(sourceDir):
        if f.lower() == filename.lower():
            # File name matches, return full path
            return os.path.join(sourceDir, f)

    return None

# Attempts to find a subpath with an unknown level of sub-directories
def findSubPath(sourceDir: str, subPath: str):
    # Normalize Windows-style separators
    normalized = subPath.replace("\\", os.path.sep)

    # Split into logical parts and remove empty / '.' / '..'
    parts = []
    for part in normalized.split(os.path.sep):
        if part == "" or part == ".":
            continue
        if part == "..":
            if parts:
                parts.pop()
        else:
            parts.append(part) 

    # So now we stripped all the . / .. bullshit out, we can actually start checking what's there
    current = sourceDir

    for part in parts:
        newCurrent = getFilenameCaseInsensitive(current, part)

        if not newCurrent:
            return None

        if not os.path.isdir(newCurrent):
            return None

        current = newCurrent

    return current

# Takes all known files and compresses them into the CAB file
def copyFilesAndWriteCab(filesInDirectory: list[SourceFile], outCab: str) -> bool:
    cabArchive = CabArchive()

    for sfEntry in filesInDirectory:
        if sfEntry.data:
            cabArchive[sfEntry.fileName] = CabFile(sfEntry.data, mtime = sfEntry.mtime)
    
    with open(outCab, 'wb') as outFile:
        outFile.write(cabArchive.save(compress = True))

    return True

# Scans a sub directory and plugs all its INF files into the INF handler
def handleDir(localDir: str, outDir: str, simulate: bool = False, deleteWin98Files: bool = False):
    infCount = 0
    filesInThisDir = list[SourceFile]()
    
    for f in os.listdir(localDir):
        fullPath = os.path.join(localDir, f)

        if os.path.exists(fullPath) and os.path.isfile(fullPath) and fullPath.lower().endswith('.inf'):
            outInf = getUniqueFilename(outDir, f)

            # If this is the first INF file in this directory, we decide the CAB name
            # Which is usually the unique output name - .inf + .cab
            # but that file name might already exist from another driver, so make sure it's unique in the same way
            if infCount == 0:
                tmpCab = os.path.splitext(f)[0] + '.cab'
                outCab = getUniqueFilename(outDir, tmpCab)
        
            logi(f'---------------------------------------------------------------')
            logi(f'Processing inf: {fullPath} outInf: {outInf} outCab: {outCab}')
            handleInf(fullPath, outInf, localDir, outCab, filesInThisDir, simulate)
            infCount += 1

    if deleteWin98Files:
        for f in filesInThisDir:
            if f.isFromWin98 and f.localName:
                loge(f'Deleting {f.localName}')
                os.remove(f.localName)

    totalSize = 0
    for f in filesInThisDir:
        if f.data: totalSize += len(f.data)
    logi(f'---> Sum of all file data (uncompressed): {totalSize} bytes')

    if infCount > 0 and not simulate:
        copyFilesAndWriteCab(filesInThisDir, outCab)
        outSize = os.path.getsize(outCab)
        logi(f'---> CAB file written: {outCab} {outSize} Bytes')

# Performs a INF analysis for one level of subdirectories in inDir
# Collects and compresses all INF's associated files into CABs
# Writes output into outDir
def driverCopy(inDir: str, outDir: str):
    if (os.path.exists(outDir)):
        shutil.rmtree(outDir)

    os.makedirs(outDir, exist_ok=True)

    # Get all Directories 
    for entry in os.listdir(inDir):
        fullEntry = os.path.join(inDir, entry)

        # if this is a directory, we process it
        if os.path.isdir(fullEntry):
            logi(f'Processing directory: {fullEntry}')
            handleDir(fullEntry, outDir)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Windows 98 QuickInstall Driver Copy Script', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--check', type=str, help='Check a directory for missing or mangled files')
    parser.add_argument('--single', nargs=2, metavar=('SRCDIR', 'OUTDIR'), help='Process a single driver (for testing purposes)')
    parser.add_argument('--delete-win98-files', type=bool, help='Delete files already supplied by Win98SE when checking', default=False)
    parser.add_argument('--verbose', help='Verbose debug logging', action='store_true', default=False)
    parser.add_argument('--decompress_szdd', nargs=2, metavar=('SRC', 'DST'))

    args = parser.parse_args()

    if args.verbose:
        ENABLE_DEBUG_LOGS = True

    if args.check:
        handleDir(args.check, '.', simulate=True, deleteWin98Files=args.delete_win98_files)

    if args.single:
        srcDir, outDir = args.single
        handleDir(srcDir, outDir)

    if args.decompress_szdd:
        src, dst = args.decompress_szdd
        dataCompressed = open(src, 'rb').read()
        data = msExpandDecompress(dataCompressed)
        open(dst, 'wb').write(data)

