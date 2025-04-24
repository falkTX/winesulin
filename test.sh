#!/bin/bash

set -e

# change to dir where this script resides in
cd $(dirname "${0}")

# build winesulin first
make -j 2 DEBUG=1

# environment variables
VST_PATH=${VST_PATH:=~/.vst:/usr/lib/vst}
WINEPREFIX=${WINEPREFIX:=~/.wine}

if [ ! -e "${WINEPREFIX}" ]; then
    echo WINEPREFIX does not exist
    exit 1
fi

# target VST2 plugins folder
TARGET_FOLDER="${WINEPREFIX}/drive_c/Program Files/VSTPlugins"
mkdir -p "${TARGET_FOLDER}"

# parse VST_PATH
IFS=':'
VST_PATHS=(${VST_PATH[@]})

# IFS=$'\n'
# for path in "${VST_PATHS[@]}"; do
#   if [ -e "${path}"/Ildaeil.vst ] && [ ! -e "${TARGET_FOLDER}"/Ildaeil.vst ]; then
#       mkdir -p "${TARGET_FOLDER}"/Ildaeil.vst
#       ln -sv "${path}"/Ildaeil.vst/* "${TARGET_FOLDER}"/Ildaeil.vst/
#       cp -v bin/winesulin.dll "${TARGET_FOLDER}"/Ildaeil.vst/Ildaeil-FX.dll
#       cp -v bin/winesulin.dll.so "${TARGET_FOLDER}"/Ildaeil.vst/Ildaeil-FX.dll.so
#       cp -v bin/winesulin.dll "${TARGET_FOLDER}"/Ildaeil.vst/Ildaeil-Synth.dll
#       cp -v bin/winesulin.dll.so "${TARGET_FOLDER}"/Ildaeil.vst/Ildaeil-Synth.dll.so
#   fi
# done
#
# # finally the real deal: put symlinks for all VST2 plugins + copy winesulin binaries
# IFS=$'\n'
# for path in "${VST_PATHS[@]}"; do
#     for p in $(ls "${path}"/ | grep "^.*\.so$" | grep -v "^.*\.dll\.so$"); do
#         if [ ! -e "${TARGET_FOLDER}"/"${p}" ]; then
#           ln -sv "${path}"/$(echo ${p} | awk 'sub(".so$",".*")') "${TARGET_FOLDER}"/
#         fi
#         cp -v bin/winesulin.dll "${TARGET_FOLDER}"/$(echo ${p} | awk 'sub(".so$",".dll")')
#         cp -v bin/winesulin.dll.so "${TARGET_FOLDER}"/$(echo ${p} | awk 'sub(".so$",".dll.so")')
#     done
# done

# ln -sfv /usr/lib/vst/drowaudio-reverb.so "${TARGET_FOLDER}"/
cp -v bin/winesulin.dll "${TARGET_FOLDER}"/drowaudio-reverb.dll
cp -v bin/winesulin.dll.so "${TARGET_FOLDER}"/drowaudio-reverb.dll.so

# TESTING
# exec wine64 "/home/falktx/.wine/drive_c/Program Files/Image-Line/FL Studio 21/FL64.exe"
exec wine64 "/home/falktx/.wine/drive_c/Program Files/Carla/Carla.exe" Z:\\home\\falktx\\Documents\\1.carxp
exec carla-single win64 vst2 "/home/falktx/.wine/drive_c/Program Files/VSTPlugins/drowaudio-reverb.dll"
