#!/bin/python3

# Makeiso for Windows 98 QuickInstall.
# Python version.
# (C) 2026 Eric Voirin (oerg866@googlemail.com)

from io import BytesIO
import pycdlib

import os


def replaceIsoInvalidChars(path: str) -> str:
    result = ''
    for ch in path.upper():
        if 'A' <= ch <= 'Z' or '0' <= ch <= '9' or ch == '_':
            result += (ch)
        else:
            result += ('_')

    return result

# Get a ISO9660 compliant path from an input path.
# ExtensionSeparator is to replace the dot in extensions
# This is helpful for creating directories where dots aren't allowed strictly speaking
def getIsoPath(path: str, extensionSeparator='.') -> str:
    parts = path.split('/')
    isoPath = ""

    for part in parts:
        if (len(part) > 0):
            pathAndExts = part.split('.')

            # Add the first part as the file/dirname, max. 8 chars
            fixed = replaceIsoInvalidChars(pathAndExts[0][:8])

            # If we have an extension, add it, but in interchange level 2 we cant use dots, so use '_'
            if len(pathAndExts) > 1:
                ext = replaceIsoInvalidChars(pathAndExts[1][:3])
                fixed += extensionSeparator + ext


            isoPath += '/'
            isoPath += fixed.upper()

    return isoPath

# Create an ISO image containing all files in inDir.
# imageFile is the output file, bootFile is relative to inDir the boot floppy image file
# label is the... label :P
def makeIso(inDir, imageFile, bootFile, label):

    if os.path.exists(imageFile):
        os.remove(imageFile)

    iso = pycdlib.PyCdlib()

    # Joliet level 3, Interchange level 2
    # So we have a bit of breathing room on some path naming limitations
    iso.new(joliet=3, interchange_level=2, vol_ident=label)

    # Add all directories and files
    for root, dirs, files in os.walk(inDir):
        rootRelative = os.path.relpath(root, inDir).replace('\\', '/')
        
        if (rootRelative.startswith('.')):
            rootRelative = rootRelative[1:]

        for d in dirs:
            jolietPath = os.path.join(rootRelative, d)
            jolietPath = '/' + jolietPath.replace('\\', '/')
            isoPath = getIsoPath(jolietPath, '_')
            iso.add_directory(isoPath, joliet_path=jolietPath)

        for f in files:
            sourcePath = os.path.join(root, f)
            jolietPath = os.path.join(rootRelative, f)
            jolietPath = '/' + jolietPath.replace('\\', '/')
            isoPath = getIsoPath(rootRelative, '_') + getIsoPath(f)

            print(f'makeIso: Adding {sourcePath}, Joliet: {jolietPath} ISO: {isoPath}')

            fileData = open(sourcePath, 'rb').read()
            iso.add_fp(BytesIO(fileData), len(fileData), isoPath, joliet_path=jolietPath)
            
            if f == bootFile:
                print(f'Boot image added ({isoPath})')
                iso.add_eltorito(isoPath, media_name='floppy')

    iso.write(imageFile)
    iso.close()
