"Paragon PNTFS/UFSD Helper Driver"

Fixes and improvements for the Paragon PNTFS.VXD / UFSD.VXD drivers
which allow external implementation.

Feature description:
1. Can set ReadOnly on NTFS volumes on fixed drives.
2. Handles multiple partitions on removable drives.
This is not mounted by default on all operating systems up to Windows 7
(10 already does).
3. Calldown Fix - fixes the bug in the TSD portion of the PNTFS/UFSD.
The bug involves multiple memory frees and, consequently, pointers to nowhere,
and can manifest itself as a BSOD (typical), a freeze, or any other glitch
when attempting to stop an NTFS volume on a fixed drive
(this is significant; the bug does not occur on removable drives,
since they run Microsoft code).

By default, all three options are enabled. Each option can be disabled via the registry:

[HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\VxD\PNTFSHLP]
"FixedReadOnly"=hex:01
"RemovableMultiple"=hex:01
"CalldownFix"=hex:01

Installation:
Copy PNTFSHLP.VXD to %windir%\SYSTEM\IOSUBSYS\ and reboot.

Removal:
Delete PNTFSHLP.VXD from %windir%\SYSTEM\IOSUBSYS\ and reboot.

See the corresponding UFSD.VXD update for patches of internal bugs.
