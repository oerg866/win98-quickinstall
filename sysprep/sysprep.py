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
import re
import fnmatch
import stat

from makeusb import make_usb
from mercypak import mercypak_pack

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

def file_exists(directory, filename):
# Check if a file with a given filename exists in a directory in a case-insensitive manner.
    for file in os.listdir(directory):
        if file.lower() == filename.lower():
            return True
    return False

def case_insensitive_to_sensitive(directory, filename):
# Gets the case sensitive name of a file in a certain directory (the directory must be in correct case already though)
    for file in os.listdir(directory):
        if file.lower() == filename.lower():
            return os.path.join(directory, file)
    return None
    

# Delete a file with a given filename in a directory in a case-insensitive manner. 'filename' may include wildcards ('*')
def delete_file(directory, filename):
    if not os.path.exists(directory):
        return

    result = False

    for file in os.listdir(directory):
        if re.match(fnmatch.translate(filename), file, re.IGNORECASE):
            try:
                print(file)
                os.remove(os.path.join(directory, file))
                result = True
            except OSError:
                return False
    return result

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

# Find Win9x directory in a base path (recursive)
def get_win_dir(osroot_path):
    for root, dirs, files in os.walk(osroot_path):
        for file_name in files:
            if file_name.upper() == 'WIN.COM'.upper():
                return root
    return None

# Find Win9x install CAB directory in a base path (recursive)
def get_cab_dir(osroot_path):
    for root, dirs, files in os.walk(osroot_path):
        for file_name in files:
            if file_name.upper() == 'PRECOPY1.CAB'.upper():
                return root
    return None

# Get the absolute path on a virtual WINE Windows environment (linux only, maybe mac?)
def get_wine_path(path):
    return subprocess.check_output(['winepath', '-w', path]).strip()  # idk why winepath appends a \n here and why it trips up everything...

# Run Windows 98's REGEDIT in 16-bit DOS emulation (bundled, sorry microsoft, don't sue me :C)
def run_regedit(reg_file):
    regedit_exe = os.path.join(script_base_path, 'registry', 'regedit.exe')
    msdos_exe = os.path.join(script_base_path, 'tools', 'msdos.exe')

    # regedit is called from *within* msdos.exe.
    
    if platform.system() == 'Windows':
        subprocess.run([msdos_exe, regedit_exe, '/L:SYSTEM.DAT', '/R:USER.DAT', reg_file], check=True, stdout=global_stdout)
    else:
        reg_file = get_wine_path(reg_file)
        regedit_exe = get_wine_path(regedit_exe)
        subprocess.run(['wine', msdos_exe, regedit_exe, '/L:SYSTEM.DAT', '/R:USER.DAT', 'tmp.reg'], check=True, stdout=global_stdout)

# Add registry file to a given windows installation and pack the registry with mercypak
def registry_add_reg(osroot_base, osroot_windir_relative, reg_file, output_866_file):
    osroot_windir_absolute = os.path.join(osroot_base, osroot_windir_relative)
    osroot_sysdir_absolute = case_insensitive_to_sensitive(osroot_windir_absolute, 'SYSTEM')
    registry_temp_path = os.path.join(script_base_path, '.regtmp')
    registry_temp_windir_absolute = os.path.join(registry_temp_path, osroot_windir_relative)

    print('Processing system registry...')

    delete_recursive(registry_temp_path)

    # Prepare directory with SYSTEM.DAT and USER.DAT files
    mkdir(registry_temp_windir_absolute)

    system_dat = case_insensitive_to_sensitive(osroot_windir_absolute, 'SYSTEM.DAT')
    user_dat = case_insensitive_to_sensitive(osroot_windir_absolute, 'USER.DAT')

    shutil.copy2(system_dat, registry_temp_windir_absolute)
    shutil.copy2(user_dat, registry_temp_windir_absolute)

    # Reboot hack, find appropriate shell32 version.

    shell32_dll = 'SHELL32.DLL'

    if file_exists(osroot_sysdir_absolute, 'SHELL32.W98'):
        shell32_dll = 'SHELL32.W98'
        print ('98Lite on Windows 98 detected.')
    elif file_exists(osroot_sysdir_absolute, 'SHELL32.WME'):
        shell32_dll = 'SHELL32.WME'
        print ('98Lite on Windows ME detected.')
    else:
        print('Stock Windows 9x detected.')

    print(f'Using {shell32_dll} to reboot!')

    pushd(registry_temp_windir_absolute)

    # Copy to temporary file and append reboot file to it
    shutil.copy2(reg_file, 'tmp.reg')
    append_line_to_file('tmp.reg', f'"Reboot"="RUNDLL32.EXE {shell32_dll},SHExitWindowsEx 2"\n')

    run_regedit('tmp.reg')
    
    os.remove('tmp.reg')

    mercypak_pack(registry_temp_path, output_866_file)

    popd()

    delete_recursive(directory_path=registry_temp_path)

# Move INF and CAB files after drivercopy processing into the relative directories they would be in after installation.
def move_inf_cab_files(directory_path, inf_directory, cab_directory):
    # Create the target directories if they do not exist
    mkdir(inf_directory)
    mkdir(cab_directory)
    
    # Loop through all files in the directory
    for file_name in os.listdir(directory_path):
        file_path = os.path.join(directory_path, file_name)
        
        # Check if the file is an INF or CAB file
        if file_name.lower().endswith('.inf'):
            # Move the file to the INF directory
            shutil.move(file_path, os.path.join(inf_directory, file_name))
        elif file_name.lower().endswith('.cab'):
            # Move the file to the CAB directory
            shutil.move(file_path, os.path.join(cab_directory, file_name))

# Runs the "drivercopy" tool, platform independent as usual with WINE magic... 
def drivercopy(source_path, output_path):
    
    drivercopy_path = os.path.join(script_base_path, 'tools', 'drivercopy.exe')

    shutil.copy2(os.path.join(script_base_path, 'tools', 'makecab.exe'), 'makecab.exe')

    if platform.system() == 'Windows':
        subprocess.run([drivercopy_path, source_path, output_path], check=True, stdout=global_stdout)       
    else:
        source_path = get_wine_path(source_path)
        output_path = get_wine_path(output_path)
        subprocess.run(['wine', drivercopy_path, source_path, output_path], check=True, stdout=global_stdout)

    os.remove('makecab.exe')

# Preprocess the slipstream + extra drivers for this sysprep run
def preprocess_drivers(output_base, input_drivers_base, input_drivers_extra):
    driver_temp = os.path.join(output_base, '.drvtmp')

    print('Preprocessing drivers...')
    
    # First do the extra drivers
    print('Preprocessing EXTRA drivers...')
    output_drivers_extra = os.path.join(output_base, 'driver.ex')
    drivercopy(input_drivers_extra, output_drivers_extra)

    # Prepare the drivers. Later we need to finalize them for each OSRoot.
    print('Preprocessing SLIPSTREAMED drivers...')
    drivercopy(input_drivers_base, driver_temp)

# Finalize the slipstream drivers for this sysprep run for a given OSRoot
def finalize_drivers_for_osroot(output_base, output_osroot, osroot_cabdir_relative):
    print('Finalizing drivers for this OSRoot...')

    # Working around a bug in drivercopy (or more specifically makecab) where the output has to be relative
    # So all the cab files go into a local directory.

    input_driver_temp = os.path.join(output_base, '.drvtmp')
    output_driver_temp = os.path.join(output_base, '.drvtmp_osroot')
    shutil.rmtree(output_driver_temp, ignore_errors=True)
    mkdir(output_driver_temp)

    shutil.copytree(input_driver_temp, output_driver_temp, dirs_exist_ok=True)

    driver_temp_cabdir = os.path.join(output_driver_temp, osroot_cabdir_relative)
    driver_temp_infdir = os.path.join(output_driver_temp, 'DRIVER')
    
    move_inf_cab_files(output_driver_temp, driver_temp_infdir, driver_temp_cabdir)

    output_866_file = os.path.join(output_osroot, 'DRIVER.866')
    mercypak_pack(output_driver_temp, output_866_file)

    shutil.rmtree(output_driver_temp)

# Cleanup after processing the drivers
def process_drivers_cleanup(output_base):
    shutil.rmtree(os.path.join(output_base, '.drvtmp'))

# Make an ISO File (TODO: Use pycdlib to remove mkisofs dependency)
def make_iso(output_base, output_iso):
    print('Creating ISO file...')
    pushd(output_base)
    if platform.system() == 'Windows':
        mkisofs_path = os.path.join(script_base_path, 'tools', 'mkisofs.exe')
    else:
        mkisofs_path = 'mkisofs' # no path on Linux & co

    subprocess.run([mkisofs_path, '-J', '-r', '-V', 'Win98 QuickInstall', '-o', output_iso, '-b', 'cdrom.img', '.'], check=True, stdout=global_stdout, stderr=global_stdout)
    popd()

#############################################################################
#
# MAIN FUNCTION STARTS HERE!!!
#
#############################################################################

# create an argument parser with options, parse and extract them
parser = argparse.ArgumentParser(description='Windows 98 QuickInstall Image Creation Script', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('--iso', type=str, help='Target filename for output ISO file')
parser.add_argument('--usb', type=str, help='Target filename for output USB image file')
parser.add_argument('--osroot', type=str, action='append', help='Path to an OS root directory (can be specified multiple times)', required=True)
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

# Preprocess drivers
preprocess_drivers(output_base, input_drivers_base, input_drivers_extra)

# Process all OSroots.
osroot_idx = 1
for osroot in input_osroots:
    osroot = os.path.realpath(osroot)
    print(f'Processing OS Root "{osroot}"')
    output_osroot = os.path.join(output_osroots_base, str(osroot_idx))
    mkdir(output_osroot)

    osroot_title = 'Windows 98 Installation'

    # Give the OSRoot a name for the installer menu

    if file_exists(osroot, 'win98qi.inf'):
        with open(case_insensitive_to_sensitive(osroot, 'win98qi.inf'), 'r') as file:
            osroot_title = file.read()
    else:
        print(f'The specified OS Root "{osroot}" does not contain a "win98qi.inf" file, which would contain the name for the installer menu.')
        osroot_title = input('Please enter the name for this installation: ')

    osroot_windir = get_win_dir(osroot)
    osroot_cabdir = get_cab_dir(osroot)

    if osroot_windir is None:
        raise ValueError("Could not find WIN.COM in directory tree")
    
    if osroot_cabdir is None:
        raise ValueError("Could not find CAB files in directory tree")

    osroot_windir_relative = os.path.relpath(osroot_windir, osroot)
    osroot_cabdir_relative = os.path.relpath(osroot_cabdir, osroot)

    print(f'Windows directory: {osroot_windir} (relative: {osroot_windir_relative})')
    print(f'Windows CAB directory: {osroot_cabdir} (relative: {osroot_cabdir_relative})')

    # Process registry
    fastpnp_reg = os.path.join(script_dir, 'registry', 'fastpnp.reg')
    fastpnp_866 = os.path.join(output_osroot, 'FASTPNP.866')
    slowpnp_reg = os.path.join(script_dir, 'registry', 'slowpnp.reg')
    slowpnp_866 = os.path.join(output_osroot, 'SLOWPNP.866')
    registry_add_reg(osroot, osroot_windir_relative, slowpnp_reg, slowpnp_866)
    registry_add_reg(osroot, osroot_windir_relative, fastpnp_reg, fastpnp_866)
#    process_osroot(osroot, osroot_idx)

    # Cleanup unnecessary files
    delete_file(osroot_windir,                              'win386.swp')
    delete_file(os.path.join(osroot_windir, 'sysbckup'),    '*')
    delete_file(os.path.join(osroot_windir, 'inf'),         'mdm*.inf')
    delete_file(os.path.join(osroot_windir, 'inf'),         'wdma_*.inf')
    delete_file(os.path.join(osroot_windir, 'inf', 'other'),'*')
    delete_file(os.path.join(osroot_windir, 'recent'),      '*')
    delete_file(osroot,                                     'win386.swp')
    delete_file(osroot,                                     'bootlog.*')
    delete_file(osroot,                                     'frunlog.txt')
    delete_file(osroot,                                     'detlog.txt')
    delete_file(osroot,                                     'setuplog.txt')
    delete_file(osroot,                                     'scandisk.log')
    delete_file(osroot,                                     'netlog.txt')
    delete_file(osroot,                                     'suhdlog.dat')
    delete_file(osroot,                                     'msdos.---')
    delete_file(osroot,                                     'config.bak')
    delete_file(osroot,                                     'autoexec.bak')

    # Copy oeminfo
    shutil.copy2(os.path.join(input_oeminfo, 'oeminfo.ini'), case_insensitive_to_sensitive(osroot_windir, 'system'))
    shutil.copy2(os.path.join(input_oeminfo, 'oemlogo.bmp'), case_insensitive_to_sensitive(osroot_windir, 'system'))

    # Finalize drivers for every package.
    finalize_drivers_for_osroot(output_base, output_osroot, osroot_cabdir_relative)

    # Do the OSROOT mercypaking now.
    mercypak_pack(osroot, os.path.join(output_osroot, 'FULL.866'))

    if not file_exists(output_osroot, 'FULL.866'):
        raise RuntimeError('There was an error. The required OSROOT pack file was not created ("FULL.866")')

    # Do the title tag file.
    with open(os.path.join(output_osroot, 'win98qi.inf'), 'w', encoding="utf-8") as file:
        file.write(osroot_title)

    osroot_idx += 1

# Copy CDROM Root stuff
print('Copying installation image base files...')
shutil.copytree(input_cdromroot, output_base, dirs_exist_ok=True)

# Copy extra CD files.
print('Copying extra CD files...')

for extradir in input_extras:
    shutil.copytree(extradir, output_extras, dirs_exist_ok=True)

process_drivers_cleanup(output_base)

print(f'Sysprep complete, output is in "{output_base}"')

# Create output images

if output_image_iso is not None:
    make_iso(output_base, output_image_iso)

if output_image_usb is not None:
    make_usb(output_base, output_image_usb)
