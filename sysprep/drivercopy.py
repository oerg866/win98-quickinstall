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
# 2026-02-03: Python rewrite, remove all CatalogFile references to prevent missing file popups during HW detection
# 2025-02-19: Fix CatalogFile entries with nonexistant files causing CAB file creation to fail (fixes ArtX)
# 2024-10-21: Fix problems with non-unique INF file names causing CAB files to be overwritten and thus missing driver files.

import os
import shutil
from struct import unpack
from cabarchive import CabArchive, CabFile
from wininfparser import WinINF, INFsection

def msExpandDecompress(data: bytes) -> bytes:
    """
    Decompress SZDD (MS COMPRESS/EXPAND) data into raw bytes.

    Expected SZDD header layout:
      0x00: 'SZDD'
      0x08: mode
      0x09: missing_char
      0x0A-0x0D: uncompressed size (uint32 little-endian)
      0x0E-...: LZSS payload

    Written by Alpon Sepriando for SATAID (MIT License)
    https://pypi.org/project/sataid/
    """
    if len(data) < 14 or data[:4] != b"SZDD":
        raise ValueError("Not an SZDD file (signature 'SZDD' not found).")

    out_len = unpack('I', data[10:14])[0]
    payload = data[14:]

    # LZSS sliding window (4096 bytes), initialized with spaces
    window = bytearray(4096)
    for i in range(4096):
        window[i] = 0x20  # space character

    pos = 4096 - 16
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
                # Sequence (offset, length)
                if p + 1 > len(payload):
                    break
                matchpos = payload[p]
                matchlen = payload[p + 1]
                p += 2

                matchpos |= (matchlen & 0xF0) << 4
                matchlen = (matchlen & 0x0F) + 3

                for _ in range(matchlen):
                    c = window[matchpos & 0xFFF]
                    matchpos = (matchpos + 1) & 0xFFF
                    out.append(c)
                    window[pos] = c
                    pos = (pos + 1) & 0xFFF

            bit <<= 1

    if out_len > 0 and len(out) >= out_len:
        out = out[:out_len]

    return bytes(out)

class SourceFile:
    def __init__(self, fileName: str, sourceDir: str):
        self.fileName = fileName    # The file name for this entry
        self.sourceDir = sourceDir  # The optional source directory for this entry

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
def getCopyFilesSections(inf: WinINF, rawFiles: list[str]):
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
                addOrUpdateSourceFile(rawFiles, cfSec[1:])
            else:
                appendIfNew(sections, cfSec)

    return sections

# Get a list of all files referenced by all CopyFiles sections in the INF
def getAllFilesFromSections(inf: WinINF, sectionNames: list[str], knownFiles: list[SourceFile]):
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

                addOrUpdateSourceFile(knownFiles, fname.strip())

def addOrUpdateSourceFile(sfList: list[SourceFile], filename: str, sourceDir:str=''):
    for sfEntry in sfList:
        # If we have this entry already AND the INF file matches, update its source dir
        if sfEntry.fileName.lower() == filename.lower():

            if sfEntry.sourceDir and sfEntry.sourceDir.lower() != sourceDir.lower():
                loge('Ambiguious source file directory')
                raise Exception(f'Source file {filename} source directory is not unique in this folder!')

            sfEntry.sourceDir = sourceDir
            return
    # Else, add a new one
    sfList.append(SourceFile(filename, sourceDir))

# Get the value string out of the SourceDisksNames section for a given source disk index
def getSourceDiskString(inf: WinINF, sourceDiskIndex: str):
    sdnSection = getSection(inf, 'SourceDisksNames')
    if not sdnSection:
        return None
    
    for key, value, comment in sdnSection:
        if key.lower() == sourceDiskIndex.lower():
            return value

    return None

# Get a list of all files referenced by the SourceDisksFiles section of this INF
def getSourceDisksFiles(inf: WinINF, knownFiles: list[SourceFile]):
    sdfSection = getSection(inf, 'SourceDisksFiles')


    if not sdfSection:
        return
    
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

        addOrUpdateSourceFile(knownFiles, key.strip(), sourceDir)

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
def handleInf(filename: str, outInfName: str, cabName: str, filesInDirectory: list[SourceFile]) -> bool:
    inf = WinINF()
    inf.ParseFile(filename)

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
    copyFilesSections = getCopyFilesSections(inf, knownFiles)

    # Gather all related files from this driver
    getAllFilesFromSections(inf, copyFilesSections, knownFiles)
    getSourceDisksFiles(inf, knownFiles)

    writeSourceDisksNamesAndFiles(inf, knownFiles, cabName)

    success = inf.Save(outInfName)

    # Wininfparser saves with \n line endings, which Win98 doesn't really like.
    # So we replace them by hand with \r\n

    allLines = open(outInfName, 'r').readlines()

    with open(outInfName, 'w') as f:
        for line in allLines:
            f.write(f'{line}\r\n')

    for newFile in knownFiles:
        addOrUpdateSourceFile(filesInDirectory, newFile.fileName, newFile.sourceDir)

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

# Gets the correct path for a SourceFile, first checkinig the specified subdirectory, and if that doesn't exist
# try the given local directory
def getSourceFileNameSourceDiskDirectoryOrLocal(sf: SourceFile, localDir: str):
    result = None
    subDir = findSubPath(localDir, sf.sourceDir)
    
    logd(f'Subdir {sf.sourceDir} {localDir} {subDir}')
    
    if subDir:
        result = getFilenameCaseInsensitive(subDir, sf.fileName)

    if not result:
        result = getFilenameCaseInsensitive(localDir, sf.fileName)
        if result:
            logw(f'Warnnig: file {sf.fileName} was expected to be found in {sf.sourceDir} but was found in {localDir}')

    return result

# Takes all known files and compresses them into the CAB file
def copyFilesAndWriteCab(sourceDir: str, outDir: str, outCab: str, win98files: list[str], filesInDirectory: list[SourceFile]) -> bool:
    cabArchive = CabArchive()

    for sfEntry in filesInDirectory:
        isFromWin98 = containsCaseInsensitive(win98files, sfEntry.fileName)
        localName = getSourceFileNameSourceDiskDirectoryOrLocal(sfEntry, sourceDir)

        if isFromWin98 and localName: 
            logw(f'WARNING: File {localName} supplied by Win9x AND driver - potential conflict!!!')

        if localName:
            logd(f'Adding {localName} to cabinet')

            # We supplied this file, add it to the cab file
            fileData = open(localName, 'rb').read()

            # We have the file data, compress and add
            cabArchive[sfEntry.fileName] = CabFile(fileData)
        elif isFromWin98:
            logd(f'File {sfEntry.fileName} supplied by Win9x!')
        else:
            # Last attempt: This file may be MS EXPAND/SZDD compressed.
            # Build a compressed file extension
            base, ext = os.path.splitext(sfEntry.fileName)
            if len(ext) == 4:
                ext = ext[:3]
            ext += '_'
            
            # now plug the extension in the original file and check if it exists
            # localCompressed = getFilenameCaseInsensitive(sourceDir, base + ext)
            compressedSourceFile = SourceFile(base + ext, sfEntry.sourceDir)
            localCompressed = getSourceFileNameSourceDiskDirectoryOrLocal(compressedSourceFile, sourceDir)

            if localCompressed:
                logd(f'File {sfEntry.fileName} was SZDD compressed, decompressing...')

                fileDataCompressed = open(localCompressed, 'rb').read()
                fileData = msExpandDecompress(fileDataCompressed)

                if len(fileData) == 0:
                    logw(f'Decompressed file {localCompressed} empty?!')

                logd(f'Adding {localCompressed} (SZDD Decompressed {len(fileData)} Bytes) to cabinet')
                # We have the file data, compress and add
                cabArchive[sfEntry.fileName] = CabFile(fileData)
            else:
                loge(f'File {sfEntry.fileName} is NOT supplied by driver nor Win9x itself!')
                raise Exception(f'Please fix {sfEntry.fileName} before packaging ISO!')

    
    with open(outCab, 'wb') as outFile:
        outFile.write(cabArchive.save(compress = True))

    return True

# Scans a sub directory and plugs all its INF files into the INF handler
def handleDir(sourceDir: str, outDir: str, win98files: str):
    infCount = 0
    filesInThisDir = []
    
    for f in os.listdir(sourceDir):
        fullPath = os.path.join(sourceDir, f)

        if os.path.exists(fullPath) and os.path.isfile(fullPath) and fullPath.lower().endswith('.inf'):
            outInf = getUniqueFilename(outDir, f)

            # If this is the first INF file in this directory, we decide the CAB name
            # Which is the unique output name - .inf + .cab
            if infCount == 0:
                outCab = os.path.splitext(outInf)[0] + '.cab'
        
            print(f'---------------------------------------------------------------')
            print(f'Processing inf: {fullPath} outInf: {outInf} outCab: {outCab}')
            handleInf(fullPath, outInf, outCab, filesInThisDir)
            infCount += 1
        
    if infCount > 0:
        copyFilesAndWriteCab(sourceDir, outDir, outCab, win98files, filesInThisDir)    
        outSize = os.path.getsize(outCab)
        logi(f'---> CAB file written: {outCab} {outSize} Bytes')

# Performs a INF analysis for one level of subdirectories in inDir
# Collects and compresses all INF's associated files into CABs
# Writes output into outDir
def driverCopy(inDir: str, outDir: str):
    allFiles = []

    # Get a list of all the Win98 supplied setup files
    win98filesDat = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'win98files.dat')

    with open("win98files.dat", "r") as f:
        win98files = f.read().splitlines()

    print(f'Starting DriverCopy with {len(win98files)} known Windows 98 files.')

    if (os.path.exists(outDir)):
        shutil.rmtree(outDir)

    os.makedirs(outDir, exist_ok=True)

    # Get all Directories 
    for entry in os.listdir(inDir):
        fullEntry = os.path.join(inDir, entry)

        # if this is a directory, we process it
        if os.path.isdir(fullEntry):
            logi(f'Processing directory: {fullEntry}')
            handleDir(fullEntry, outDir, win98files)


#driverCopy('/work/win98-quickinstall/win98-driver-lib-base/', '.drvtmp3')