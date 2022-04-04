#!/bin/bash

#    afs_build_all.sh is a bash script for linux (Debian) that builds a 
#    fully working cf card from a freshly compiled AROS/ApolloOS
#    (repository: https://github.com/ApolloTeam-dev/AROS)
#    Copyright (C) RedBug  (Contact: Apollo Discord channel @RedBug)
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
#    ADDITIONAL LICENSE INFORMATION
#
#    All build scripts (afs_clean_all.sh, afs_detect_dev.sh etc.) and other
#    materials included (pregenerated empty images, documentation etc.) if not
#    otherwise stated are also pulished under the free GPL license.
#    (Meaning: EVERYTHING is Free Software and can be reused, modified etc.
#    as stated in the GPL license.)
#
#    USAGE:
#    ./afs_build.all.sh device [size|quick]
#    device = /dev/sdx (sdb, sdc, sdd etc. that refers to the cf card)
#    size = 2 or 4 (size in GB of image that will be built)
#    quick = if "quick" is used instead of size prepartitioned / preformatted
#    cf card is used (faster and less "dangerous" = less risk of data loss)

#    The parameters device and size are optional
#    - if size is omitted a 2GB image will be built
#    - if device is omitted the script will ask you to disconnect / reconnect
#      your cf card to figure out automatically how it is connected
#   
#    EXAMPLES:
#    ./afs_clean_all.sh
#    ./afs_build_all.sh /dev/sdb 4
#    ./afs_build_all.sh /dev/sdc 2
#    ./afs_build_all.sh /dev/sde quick
#    Builds a 4GB image on /dev/sdb, a 2GB image on /dev/sdc or uses a
#    prepartitioned/preformatted cf card in /dev/sde
#    (./afs_clean_all.sh can be used beforehand to make sure no old files
#    are included.)
#
#    DISCLAIMER: IF USED IN A WRONG WAY THIS SCRIPT MAY DESTROY YOUR HARDDISK.
#    I AM NOT RESPONSIBLE FOR ANY DATA YOU MAY LOSE USING THIS SCRIPT!!!

# copy freshly compiled AROS (default directory ../ApolloOS_Master/)
# the two directions AROS.HUNK and AROS.PROGRAM will correspond to
# the devices AROS: and PROGRAMS: in AmigaOS
cp -r ../ApolloOS_Master/bin/amiga-m68k/AROS.HUNK .
mkdir AROS.PROGRAMS
# copy aros.rom (1MB) to aros:boot so that it can be mapped on vampire
cp -v ../ApolloOS_Master/aros.rom ../ApolloOS_Master/bin/amiga-m68k/AROS.HUNK/boot/
# download 3rd party packages (only a few for the moment)
mkdir 3rd_party
cd 3rd_party
echo "Download and extract 3rd party components ..."
# list of packages
packages=(
    http://www.apollo-core.com/downloads/SAGADriver.lha
    https://aminet.net/comm/tcp/smbfs-68k.lha
    https://aminet.net/comm/tcp/TwinVNC0.8beta.lha
)
for pkg in "${packages[@]}"
do
    # download    
    wget "$pkg"
    # extract    
    name=$(basename $pkg)
    lha x $name    
    #echo "Basename: $name"    
done
mkdir "lha"
mv *.lha ./lha/
echo "Copy files from source to destination ..."
cd ..
# First define file list copy function
CopyFileList () {
    for file in "${list[@]}"
    do
        mv -v "$src$file" "$dest"
    done
}
# SAGADriver 
# C
src="./3rd_party/SAGADriver/C/"
dest="./AROS.HUNK/C/"
list=(
    I2Clock    
    SDDiag
    VampireFlash
    VampireMap
    VampireSN.exe
    VControl
)
CopyFileList
# Devs [v4net.device]
src="./3rd_party/SAGADriver/Devs/Networks/"
dest="./AROS.HUNK/Devs/Networks/"
list=(
    v4net.device
)
CopyFileList
# Devs [i2c.library]
src="./3rd_party/SAGADriver/Libs/"
dest="./AROS.HUNK/Libs/"
list=(
    i2c.library
)
CopyFileList
# smbfs
cp -v ./3rd_party/smbfs-1.74-68k/smbfs ./AROS.HUNK/C/
mkdir ./AROS.PROGRAMS/Internet
cp -r -v ./3rd_party/smbfs-1.74-68k ./AROS.PROGRAMS/Internet/
# TwinVNC
cp -r -v ./3rd_party/TwinVNC0.8beta ./AROS.PROGRAMS/Internet/
# mount amiga affs partitions and copy the files
# prepare directories to mount partitions
mkdir cf_card
mkdir cf_card/aros
mkdir cf_card/programs
# check if quick mode (= prepartitioned / preformatted cf card) is used
if [ "$2" = "quick" ]; then
    echo "Using prepartitioned / preformatted cf card ..."
else
    # use pregenerated (empty) image
    # extract image
    if [ "$2" = "4" ]; then
        hdf="afs_4GB_empty.hdf"
    else
        hdf="afs_2GB_empty.hdf"
    fi
    cd cf_card
    cp "../resources/$hdf.zip" .
    unzip $hdf
    cd ..
fi
# device for connected cf card 
if [ -z "$1" ]; then
    # detect cf card
    echo "Find out to which device your cf card is connected."
    echo "Unmount and disconnect your cf card ... <press return>"
    read key
    lsblk
    lsblk > s1.txt
    echo
    echo "Now insert/reinsert/reconnect your cf card and wait a few seconds (until there is no activity any more on your cf card reader) ... <press return>"
    read key
    lsblk
    lsblk > s2.txt
    dev=$(diff s1.txt s2.txt | grep "sd[abcd][^1234]" | sed 's/^.*\(sd[a-f]\).*$/\1/')
    if [ -z "$dev" ]; then 
        echo
        echo "That didn't work ..."
        echo "Enter device manually: "
        read dest
    else
        echo
        echo "Your cf card is connected to:"
        dest="/dev/$dev"
        echo "$dest"
    fi
else
    echo "Device has been given as command line parameter:"
    dest=$1
    echo "DEVICE=$dest"
fi
if [ "$2" != "quick" ]; then
    # write empty image to cf card
    echo
    echo "WARNING: MAKE SURE THE DESTINATION IS CORRECT! IF IT IS WRONG YOU LOSE"
    echo "ALL (AND ALL MEANS ALL!) DATA ON THAT DRIVE / CF CARD!"
    echo "I AM NOT RESPONSIBLE FOR ANY DATA YOU LOSE!"
    echo "IF YOU KNOW YOU ARE DOING PRESS RETURN"
    echo "IF YOU WANT TO STOP HERE PRESS CTRL-C!"
    read key
    sudo dd if="./cf_card/$hdf" of=$dest bs=1M status=progress
fi
# change write permissions to cf card
sudo chmod 666 $dest
# define partitions
aros_partition="$dest\1"
programs_partition="$dest\2"
# mount partitions
echo "Mounting partitions AROS: and PROGRAMS: ..."
sudo mount $aros_partition ./cf_card/aros/ -t affs
sudo mount $programs_partition ./cf_card/programs/ -t affs
# show directories to control if mount was successfull (can be commented out)
dir cf_card/aros/
dir cf_card/programs/
# if quick option is used => delete all files first
if [ "$2" = "quick" ]; then
    echo "Quick mode: delete all existing data in AROS: and PROGRAMS: first <key + return>"
    read key
    sudo rm -r -v /cf_card/aros/*
    sudo rm -r -v /cf_card/programs/*
fi
# copy all files to cf card
sudo cp -r -v ./AROS.HUNK/* cf_card/aros/
sudo cp -r -v ./AROS.PROGRAMS/* cf_card/programs
# unmount partition
sudo umount $aros_partition
sudo umount $programs_partition 
# end
echo "Wait until there is no more activity on your cf card (e.g. blinking lights on card reader)."
echo "Check if there were any errors during execution of the script (if yes, something hasn't worked correctly)"
echo "Afterwards (if there are no errors), take cf card out, insert it into your Vampire and fire it up ..."
echo "With a little bit of luck it should boot ... ;-)"
