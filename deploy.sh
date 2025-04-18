#!/bin/bash

set -e

cd $(dirname "${0}")

LXVSTDIR="/usr/lib/vst"
WINVSTDIR="$(pwd)/plugins"

mkdir -p "${WINVSTDIR}"

for p in $(find "${LXVSTDIR}" -type f -name "*.so"); do
    f=$(basename ${p} | awk 'sub(".so","")')

    # symlink main plugin file
    [ -e "${WINVSTDIR}/${f}.so" ] || ln -sfv "${p}" "${WINVSTDIR}/"

    # symlink resources used by plugin, if present
    b=$(echo ${p} | awk 'sub(".so",".bundle")')
    [ -e "${b}" ] && [ ! -e "${WINVSTDIR}/${f}.bundle" ] && ln -sv "${b}" "${WINVSTDIR}/"

    # direct copy of our plugin wrapper
    cp -v bin/winesulin.dll "${WINVSTDIR}/${f}.dll"
    cp -v bin/winesulin.dll.so "${WINVSTDIR}/${f}.dll.so"
done

# for p in $(find "${WINVSTDIR}" -type f -name "*.so" | grep -v dll); do
#     f=$(basename ${p} | awk 'sub(".so","")')
# 
# #     # symlink main plugin file
# #     [ -e "${WINVSTDIR}/${f}.so" ] || ln -sfv "${p}" "${WINVSTDIR}/"
# # 
# #     # symlink resources used by plugin, if present
# #     b=$(echo ${p} | awk 'sub(".so",".bundle")')
# #     [ -e "${b}" ] && [ ! -e "${WINVSTDIR}/${f}.bundle" ] && ln -sv "${b}" "${WINVSTDIR}/"
# # 
#     # direct copy of our plugin wrapper
#     cp -v winelinvst.dll "${WINVSTDIR}/${f}.dll"
#     cp -v winelinvst.dll.so "${WINVSTDIR}/${f}.dll.so"
# done
