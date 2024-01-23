#!/bin/bash

# Copyright Epic Games, Inc. All Rights Reserved.

_error()   { printf "\x1b[91m!! ERROR: %s\x1b[0m\n" "$1" ; }
_header()  { printf "\x1b[96m-- %s\x1b[0m\n" "$1" ; }
_success() { printf "\x1b[92m%s\x1b[0m\n" "$1" ; }

py_ver_maj=3
py_ver_min=12
py_ver=$py_ver_maj.$py_ver_min

# If we've already a destination we're done here
if [ -z "$1" ]; then
    _error "Missing 'working dir' argument"
    exit 1
fi

# If we've already a destination we're done here
dest_dir=$1/python/$py_ver
if [ -d "$dest_dir" ]; then
    exit 0
fi

# Check the versions good enough
function _check_version()
{
    # Bash's here document syntax is ugly
    cat << EOF | $1 -EsSB
import sys
vi = sys.version_info
raise SystemExit(vi.major != ${py_ver_maj} or vi.minor < ${py_ver_min})
EOF
    return $?
}

function _bin_select()
{
    py_bin=
    for candidate in "$@"; do
        if ! command -v $candidate; then
            continue
        fi

        if ! _check_version $candidate; then
            continue
        fi

        py_bin=$candidate
        return 0
    done
    return 1
}

_header "Looking for a $py_ver Python binary"
_bin_select \
    $USHELL_PY_BIN \
    python$py_ver_maj \
    /usr/local/opt/python@${py_ver_maj}.*/bin/python$py_ver_maj \
    /usr/bin/python$py_ver_maj.*[0-9] \
    /usr/local/bin/python$py_ver_maj.*[0-9] \
    python

if [ -z "$py_bin" ]; then
    _error "No suitable binary found that is $py_ver or newer"
    echo ""
    echo "Examples to install Python $py_ver:"
    echo "   Ubuntu: apt install python$py_ver"
    echo "      Mac: brew install python@$py_ver"
    echo ""
    exit 1
fi

_success "Found: $py_bin: (as $($py_bin --version))"

# Make sure that the Python install has Pip installed
_header "Checking for Pip"
curl -L https://bootstrap.pypa.io/get-pip.py | $py_bin
if [ "$?" -ne "0" ]; then
    _error "Failed to ensure Pip is installed"
    exit 1
fi

# Create the Python virtual env
_header "Fetching virtual env"

temp_dir=$1/python/${py_ver}_$$

if ! $py_bin -m pip install virtualenv; then
    _error "Unable to install 'virtualenv' Pip package"
    _error "Please ensure Pip is installed"
    exit 1
fi

_header "Creating a virtualenv in $temp_dir"
if ! $py_bin -m virtualenv $temp_dir; then
    _error "Virtualenv creation failed"
    exit 1
fi

# Verify the virtualenv is the version we expect
_check_version $temp_dir/bin/python
if [ "$?" -ne "0" ]; then
    _error "Virtualenv is an unexpected version"
    exit 1
fi

# Swap the temp dir into place. If we fail someone else did it first
if mv $temp_dir $dest_dir; then
    current_dir=$(dirname $dest_dir)/current
    _header "Symlinking $dest_dir to $current_dir"
    rm $current_dir 2>/dev/null
    ln -s $dest_dir $current_dir
else
    rm -rf $temp_dir
fi

_success Done!
echo
echo
