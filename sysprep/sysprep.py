#!/bin/python3

# Sysprep for Windows 98 QuickInstall.
# Python version.
# (C) 2023 Eric Voirin (oerg866@googlemail.com)

import argparse
import os
import errno
import subprocess
import platform
import shutil
import fnmatch
import stat

from FATtools import Volume, FAT
from makeusb import make_usb
from mercypak import mercypak_pack
from drivercopy import driverCopy
from makeiso import makeIso

# Store the current working directory in a global variable
cwd_stack = [os.getcwd()]

# Global script basepath
script_base_path = os.path.dirname(os.path.abspath(__file__))

# Global output verbose
global_stdout=subprocess.DEVNULL

def pushd(directory):
    os.chdir(directory)
    cwd_stack.append(os.getcwd())

def popd():
    if len(cwd_stack) > 1:
        cwd_stack.pop()
        os.chdir(cwd_stack[-1])

def case_insensitive_to_sensitive(fs: FAT.Dirtable, directory, filename):
# Gets the case sensitive name of a file in a certain directory (the directory must be in correct case already though)
    newdir = fs.opendir(directory)
    for file in newdir.listdir():
        if file.lower() == filename.lower():
            return os.path.join(directory, file)
    return os.path.join(directory, filename)

# Create directory, ignore if it already exists
def mkdir(path):
    try:
        os.makedirs(path, exist_ok=True)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

# Error handling for read-only files by changing their permissions and trying again
def handle_remove_readonly(func, path, exc_info):
    if not os.access(path, os.W_OK):
        os.chmod(path, stat.S_IWUSR)
        os.chmod(path, stat.S_IWRITE)
        func(path)
    else:
        raise OSError("Unable to delete file: %s" % path)

# Delete recursive... more or less just a wrapper around shutil.rmtree
def delete_recursive(directory_path):
    try:
        shutil.rmtree(directory_path, onerror=handle_remove_readonly)
    except FileNotFoundError:
        pass    # We're already deleted. not a problem!

# Append a line to a text file.
def append_line_to_file(filename, line):
    with open(filename, "a") as f:
        f.write(line + "\n")

def find_recursive_and_get_parent(fs: FAT.Dirtable, to_find):
    result = None
    for root, dirs, files in fs.walk():
        for file_name in files:
            if file_name.upper() == to_find.upper():
                result = root
    if result.startswith("./"):
        result = result[2:]
    return result

# Find Win9x directory in an image (recursive)
def get_win_dir(fs: FAT.Dirtable):
    return find_recursive_and_get_parent(fs, 'WIN.COM')

# Find Win9x install CAB directory in an image (recursive)
def get_cab_dir(fs: FAT.Dirtable):
    return find_recursive_and_get_parent(fs, 'PRECOPY1.CAB')

# Get all files from an image
def get_full_file_list(fs: FAT.Dirtable) -> list[str]:
    ret = []
    for root, dirs, files in fs.walk():
        for name in files:
            full_path = os.path.join(root, name)            
            if full_path.startswith("./"):
                full_path = full_path[2:]
            ret.append(full_path)
    return ret

# Delete from file list with pattern
def remove_from_file_list_if_present(files, subdirs, pattern, exceptions = []):
    if subdirs:
        full_pattern = os.path.join(*subdirs, pattern).lower()
    else:
        full_pattern = pattern.lower()
    
    # Make a lowercase set of exception filenames for fast lookup
    except_lower = set(name.lower() for name in exceptions)

    new_files = []

    for f in files:
        f_lower = f.lower()

        # If it matches the pattern AND is not in the exception list, remove it
        if fnmatch.fnmatch(f_lower, full_pattern):
            if f_lower not in except_lower:
                continue

        new_files.append(f)

    files[:] = new_files

# Get the absolute path on a virtual WINE Windows environment (linux only, maybe mac?)
def get_wine_path(path):
    return subprocess.check_output(['winepath', '-w', path]).strip()  # idk why winepath appends a \n here and why it trips up everything...

# Run Windows 98's REGEDIT in 16-bit DOS emulation (bundled, sorry microsoft, don't sue me :C)
def run_regedit(reg_file):
    regedit_exe = os.path.join(script_base_path, 'registry', 'regedit.exe')
    msdos_exe = os.path.join(script_base_path, 'tools', 'msdos.exe')

    # regedit is called from *within* msdos.exe.

    if platform.system() == 'Windows':
        subprocess.run([msdos_exe, regedit_exe, '/L:SYSTEM.DAT', '/R:SYSTEM.DAT', reg_file], check=True, stdout=global_stdout)
    else:
        reg_file = get_wine_path(reg_file)
        regedit_exe = get_wine_path(regedit_exe)
        subprocess.run(['wine', msdos_exe, regedit_exe, '/L:SYSTEM.DAT', '/R:SYSTEM.DAT', reg_file], check=True, stdout=global_stdout)

# Copy a file out of a FAT image, does not preserve attributes and datetime
def image_copy_out(fs: FAT.Dirtable, filename, output):
    f = fs.open(filename)
    data = f.read()

    with open(output, 'wb') as out:
        out.write(data)

def image_file_exists(fs: FAT.Dirtable, subdir, file):
    newdir = fs.opendir(subdir)
    if (newdir.find(file) == None):
        return False
    
    return True



# Add registry file to a given windows installation and pack the registry with mercypak
def registry_add_reg(fs: FAT.Dirtable, windir, reg_file, output_866_file):
#    osroot_windir_absolute = os.path.join(osroot_base, osroot_windir_relative)
#    osroot_sysdir_absolute = case_insensitive_to_sensitive(osroot_windir_absolute, 'SYSTEM')

    regtmp = os.path.join(script_base_path, '.regtmp')
    regtmp_windir = os.path.join(regtmp, windir)

    print('Processing system registry...')
    delete_recursive(regtmp)

    # Prepare directory with SYSTEM.DAT and USER.DAT files
    mkdir(regtmp_windir)


    system_dat_in = case_insensitive_to_sensitive(fs, windir, 'SYSTEM.DAT')
    system_dat_out = os.path.join(regtmp, system_dat_in)

    print(f'system.dat in = {system_dat_in}, windir {regtmp_windir}')
    image_copy_out(fs, system_dat_in, system_dat_out)

    # Reboot hack, find appropriate shell32 version.

    shell32_dll = 'SHELL32.DLL'
    system_dir = case_insensitive_to_sensitive(fs, windir, 'SYSTEM')
    if image_file_exists(fs, system_dir, 'SHELL32.W98'):
        shell32_dll = 'SHELL32.W98'
        print('98Lite on Windows 98 detected.')
    elif image_file_exists(fs, system_dir, 'SHELL32.WME'):
        shell32_dll = 'SHELL32.WME'
        print('98Lite on Windows ME detected.')
    elif image_file_exists(fs, system_dir, 'SHELL32.DLL'):
        print('Stock Windows 9x detected.')
    else:
      raise Exception("SHELL32 not found in this filesystem")
        

    print(f'Using {shell32_dll} to reboot!')

    # Copy to temporary file and append reboot file to it
    pushd(regtmp_windir)
    tmp_reg_file = os.path.join(regtmp, 'tmp.reg')
    shutil.copy2(reg_file, tmp_reg_file)
    append_line_to_file(tmp_reg_file, f'[HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce]')
    append_line_to_file(tmp_reg_file, f'"Reboot"="RUNDLL32.EXE {shell32_dll},SHExitWindowsEx 2"\n')

    run_regedit(tmp_reg_file)
    
    os.remove(tmp_reg_file)

    mercypak_pack(output_866_file, local_files=regtmp)

    popd()
    delete_recursive(regtmp)

from drivercopy import driverCopy

# Preprocess the slipstream + extra drivers for this sysprep run
def preprocess_drivers(output_base, input_drivers_base, input_drivers_extra):
    print('Preprocessing drivers...')

    # First do the extra drivers
    print('Preprocessing EXTRA drivers...')
    output_drivers_extra = os.path.join(output_base, 'driver.ex')
    driverCopy(input_drivers_extra, output_drivers_extra)

    # Prepare the base drivers. Later we need to finalize them for each OSRoot.
    print('Preprocessing SLIPSTREAMED drivers...')
    driverCopy(input_drivers_base, '.driver_int')

# Finalize the slipstream drivers for this sysprep run for a given OSRoot
def finalize_drivers_for_osroot(output_base, output_osroot, osroot_cabdir_relative):
    print('Finalizing drivers for this OSRoot...')

    # The problem is that the cab files need to be in the WinCD cabinet directory so
    # that win98 can find it without prompting, so for each OSRoot we have to copy the files once more
    # infs into INF dir *and* cabs into Win CD dir

    input_driver_temp = '.driver_int'
    output_driver_temp = '.drvtmp_osroot'
    driver_temp_cabdir = os.path.join(output_driver_temp, osroot_cabdir_relative)
    driver_temp_infdir = os.path.join(output_driver_temp, 'DRIVER')
    output_866_file = os.path.join(output_osroot, 'DRIVER.866')

    shutil.rmtree(output_driver_temp, ignore_errors=True)
    mkdir(driver_temp_cabdir)
    mkdir(driver_temp_infdir)

    # Loop through all files in the input directory
    for file_name in os.listdir(input_driver_temp):
        full_path = os.path.join(input_driver_temp, file_name)
        # Check if the file is an INF or CAB file
        if file_name.lower().endswith('.inf'):
            # Move the file to the INF directory
            shutil.copy(full_path, os.path.join(driver_temp_infdir, file_name))
        elif file_name.lower().endswith('.cab'):
            # Move the file to the CAB directory
            shutil.move(full_path, os.path.join(driver_temp_cabdir, file_name))

    mercypak_pack(output_866_file, None, None, output_driver_temp, False)

#############################################################################
#
# MAIN FUNCTION STARTS HERE!!!
#
#############################################################################

# create an argument parser with options, parse and extract them
parser = argparse.ArgumentParser(description='Windows 98 QuickInstall Image Creation Script', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('--iso', type=str, help='Target filename for output ISO file')
parser.add_argument('--usb', type=str, help='Target filename for output USB image file')
parser.add_argument('--osroot',nargs=2, metavar=('PATH', 'LABEL'), action='append', required=True,
    help='Path to an OS root image and its label (can be specified multiple times)')
parser.add_argument('--extra', type=str, action='append', help='Path to extra files to be added to the output image\'s "extras" directory (can be specified multiple times)', default=['_EXTRA_CD_FILES_'])
parser.add_argument('--drivers', type=str, help='Path to base drivers to slipstream.', default='_DRIVER_')
parser.add_argument('--extradrivers', type=str, help='Path to drivers to be added to the output image\'s "driver.ex" directory. These are *NOT* slipstreamed.', default='_EXTRA_DRIVER_')
parser.add_argument('--verbose', type=bool, help='Be verbose (show output of subprocesses)', default=False)

args = parser.parse_args()

# extract the values of the command-line options
output_image_iso = args.iso
output_image_usb = args.usb

if output_image_iso is not None:
    output_image_iso = os.path.abspath(output_image_iso)

if output_image_usb is not None:
    output_image_usb = os.path.abspath(output_image_usb)

input_osroots = args.osroot
input_extras = args.extra
input_drivers_base = os.path.abspath(args.drivers)
input_drivers_extra = os.path.abspath(args.extradrivers)

if args.verbose:
    global_stdout = None

# add the default value to the extras list if it's not already there
if '_EXTRA_CD_FILES_' not in input_extras:
    input_extras.append('_EXTRA_CD_FILES_')

script_dir = os.path.dirname(os.path.abspath(__file__))
output_base = os.path.join(script_dir, '_OUTPUT_')
output_osroots_base = os.path.join(output_base, 'osroots')
output_regtmp = os.path.join(script_dir, '.regtmp')
output_oemtmp = os.path.join(script_dir, '.oemtmp')
output_extras = os.path.join(output_base, 'extras')
input_cdromroot = os.path.join(script_dir, 'cdromroot')
input_oeminfo = os.path.join(script_dir, '_OEMINFO_')

print('Output root directory: ' + str(output_base))
print('Output image ISO: ' + str(output_image_iso))
print('Output image USB: ' + str(output_image_usb))
print('Input OS roots: ' + str(input_osroots))
print('Input Extra files: ' + str(input_extras))
print('Input Base Drivers: ' + str(input_drivers_base))
print('Input Extra Drivers: ' + str(input_drivers_extra))

shutil.rmtree(output_base, ignore_errors=True)

mkdir(output_base)
mkdir(output_regtmp)
mkdir(output_oemtmp)

# Preprocess drivers
preprocess_drivers(output_base, input_drivers_base, input_drivers_extra)

# Process all OSroots.
osroot_idx = 1
for osroot, osroot_name in input_osroots:
    osroot = os.path.realpath(osroot)
    print(f'Processing OS Root image file: "{osroot}"')
    output_osroot = os.path.join(output_osroots_base, str(osroot_idx))
    mkdir(output_osroot)
    
    # Give the OSRoot a name for the installer menu
    with open(os.path.join(output_osroot, 'wini98qi.inf'), 'w') as labelFile:
        labelFile.write(f'{osroot_name}\n')

    root = Volume.vopen(osroot, 'r+b', what='partition0')
    fs = Volume.openvolume(root)

    osroot_windir = get_win_dir(fs)
    osroot_cabdir = get_cab_dir(fs)

    if osroot_windir is None:
        raise ValueError("Could not find WIN.COM in directory tree")

    if osroot_cabdir is None:
        raise ValueError("Could not find CAB files in directory tree")

    print(f'Windows directory: {osroot_windir}')
    print(f'Windows CAB directory: {osroot_cabdir}')

    # Process registry
    fastpnp_reg = os.path.join(script_dir, 'registry', 'fastpnp.reg')
    fastpnp_866 = os.path.join(output_osroot, 'FASTPNP.866')
    slowpnp_reg = os.path.join(script_dir, 'registry', 'slowpnp.reg')
    slowpnp_866 = os.path.join(output_osroot, 'SLOWPNP.866')

    registry_add_reg(fs, osroot_windir, slowpnp_reg, slowpnp_866)
    registry_add_reg(fs, osroot_windir, fastpnp_reg, fastpnp_866)

    # Get a list of all the files in the image
    osroot_files = get_full_file_list(fs) 

    osroot_infdir = case_insensitive_to_sensitive(fs, osroot_windir, 'inf')

    remove_from_file_list_if_present(osroot_files, [osroot_infdir], 'mdm*.inf', ['windows/inf/mdmgen.inf'])
    remove_from_file_list_if_present(osroot_files, [osroot_infdir], 'wdma_*.inf', ['windows/inf/wdma_usb.inf'])

    remove_from_file_list_if_present(osroot_files, [osroot_windir], 'win386.swp')
    remove_from_file_list_if_present(osroot_files, [osroot_windir], 'ndislog.txt')
    remove_from_file_list_if_present(osroot_files, [osroot_windir], '*.log')
    remove_from_file_list_if_present(osroot_files, [osroot_infdir], 'drv*.bin')
    
    remove_from_file_list_if_present(osroot_files, [osroot_windir, 'recent'], '*')
    remove_from_file_list_if_present(osroot_files, [osroot_windir, 'temp'], '*')
    remove_from_file_list_if_present(osroot_files, [osroot_windir, 'applog'], '*')
    remove_from_file_list_if_present(osroot_files, [osroot_windir, 'sysbckup'], '*')
    remove_from_file_list_if_present(osroot_files, [osroot_windir, 'other'], '*')
    remove_from_file_list_if_present(osroot_files, ['recycled'], '*')
    
    remove_from_file_list_if_present(osroot_files, [], 'win386.swp')
    remove_from_file_list_if_present(osroot_files, [], 'bootlog.*')
    remove_from_file_list_if_present(osroot_files, [], 'frunlog.txt')
    remove_from_file_list_if_present(osroot_files, [], 'detlog.txt')
    remove_from_file_list_if_present(osroot_files, [], 'setuplog.txt')
    remove_from_file_list_if_present(osroot_files, [], 'scandisk.log')
    remove_from_file_list_if_present(osroot_files, [], 'netlog.txt')
    remove_from_file_list_if_present(osroot_files, [], 'suhdlog.dat')
    remove_from_file_list_if_present(osroot_files, [], 'msdos.---')
    remove_from_file_list_if_present(osroot_files, [], 'config.bak')
    remove_from_file_list_if_present(osroot_files, [], 'autoexec.bak')
    remove_from_file_list_if_present(osroot_files, [], 'io.bak')
    remove_from_file_list_if_present(osroot_files, [], 'command.dos')
    remove_from_file_list_if_present(osroot_files, [], 'videorom.bin')

    # No need to pack the registry, it's already included in SLOWPNP and FASTPNP
    remove_from_file_list_if_present(osroot_files, [osroot_windir], 'system.dat')

    # Copy oeminfo
    if os.path.exists(input_oeminfo):
        tmp_system_dir = os.path.join(output_oemtmp, case_insensitive_to_sensitive(fs, osroot_windir, 'system'))
        mkdir(tmp_system_dir)
        shutil.copy2(os.path.join(input_oeminfo, 'oeminfo.ini'), tmp_system_dir)
        shutil.copy2(os.path.join(input_oeminfo, 'oemlogo.bmp'), tmp_system_dir)
       
    output_osroot_full866 = os.path.join(output_osroot, 'FULL.866')
    mercypak_pack(output_osroot_full866, fs, osroot_files, output_oemtmp, mercypak_v2=True)
    
    if not os.path.exists(output_osroot_full866):
        raise Exception('There was an error. The required OSROOT pack file was not created ("FULL.866")')

    finalize_drivers_for_osroot(output_base, output_osroot, osroot_cabdir)

# Copy CDROM Root stuff
print('Copying installation image base files...')
shutil.copytree(input_cdromroot, output_base, dirs_exist_ok=True)

# Copy extra CD files.
print('Copying extra CD files...')

for extradir in input_extras:
    shutil.copytree(extradir, output_extras, dirs_exist_ok=True)

print(f'Sysprep complete, output is in "{output_base}"')

# Create output images

label = None
if output_image_iso is not None:
    if label is None:
        label = 'QuickInstall'
    makeIso(output_base, output_image_iso, 'cdrom.img', label)

if output_image_usb is not None:
    make_usb(output_base, output_image_usb)
