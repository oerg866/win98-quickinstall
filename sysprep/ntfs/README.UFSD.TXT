Paragon UFSD patches and feature descriptions (mainly applicable to NTFS).

Fixes:
1. Implemented support for non-zero Unicode pages in the volume name,
i.e. NLS in the volume label.
2. Fixed buggy DBCS handling when using the system function
for converting Unicode to DBCS (hieroglyphs are now visible
in the volume name).
3. Fixed a bug (uninitialized variable) in the Volume ID retrieval handler.
Because of this, the Volume ID was different in different retrieval procedures.
4. The algorithm for retrieving the NTFS Volume ID has been aligned with
Windows NT NTFS driver, so the IDs are now the same in both operating systems.
5. Added resource release on unmounting volumes processed by
UFSD. Yes, they weren't freed at all including the multi-megabyte cache
for the NTFS cluster bitmap on volumes with a large number of clusters
(up to 512 MiB).

Description of features:
1. Now determined why the [NTFS] volume label isn't displayed when executing
the DIR command and why some older (non Win32) applications
(which check for the existence of files or directories via search)
don't work correctly:
The driver simply lacks (doesn't have at all) a DOS-style search handler
(FS_SearchFile).
2. The same for infinite I/O speed in measurement applications with direct
volume access. According handler (FS_DirectDiskIO) is just a stub.

It's also recommended to pay attention to the related PNTFSHLP project.
