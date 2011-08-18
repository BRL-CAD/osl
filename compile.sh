# ------------------------------------------------------------------------------
# This script compiles osl and its dependencies. It's possible to
# compile a single library passing its name as parameter. If no
# parameter is provided, all libraries are compiled.
#
# Assuming installed libs:
#    * LLVM (only tested with 2.9)
#
# Targets:
#
# - all (default) -- install all libraries
# - zlib
# - ilmbase
# - openexr
# - png
# - tiff
# - jpeg
# - boost
# - oiio
# - osl
#
# ------------------------------------------------------------------------------

# Detect plataform
unamestr=`uname`

# Adjust linker path
if [[ "$unamestr" == 'Linux' ]]; then
    echo "TODO..."
elif [[ "$unamestr" == 'FreeBSD' ]]; then
    echo "TODO..."
elif [[ "$unamestr" == 'Darwin' ]]; then
# Mac OS
    export DYLD_LIBRARY_PATH=$DIR/$prefix/lib:$DYLD_LIBRARY_PATH
fi

# Adjust cmake path, so that it will search for our local builds
export CMAKE_PREFIX_INSTALL=$Dir/$prefix/:$CMAKE_PREFIX_INSTALL
# FIXME: add the location to brl-cad built libraries

# ------------------------------------------------------------------------------
# Variables
# ------------------------------------------------------------------------------
prefix=dist/

# Dir where this script is stored
DIR="$( cd "$( dirname "$0" )" && pwd )"
# Dir where we'll install everything
echo "Installing dir: $DIR/$prefix"

# Read input parameters (select which libs are to be build)
if [ -z "$1" ]; then
    param='all'
else 
    param=$1
fi

# ------------------------------------------------------------------------------ 
# Install libz library
# ------------------------------------------------------------------------------
if [[ "$param" == 'zlib' ]] || [[ "$param" == 'all' ]]; then
    echo "Installing zlib"
    cd libz; cmake -DCMAKE_INSTALL_PREFIX=$DIR/$prefix; make; make install; 
    cd ..
fi

# ------------------------------------------------------------------------------
# Install ilmbase
# ------------------------------------------------------------------------------
if [[ "$param" == 'ilmbase' ]] || [[ "$param" == 'all' ]]; then
    echo "Installing ilmbase"
    cd ilmbase; ./configure --prefix=$DIR/$prefix; make; make install
    cd ..
fi

export ILMBASE_HOME=$DIR/$prefix

# ------------------------------------------------------------------------------
# Install openexr
# ------------------------------------------------------------------------------
if [[ "$param" == 'openexr' ]] || [[ "$param" == 'all' ]]; then
    echo "Installing openexr"
    cd openexr; ./configure --prefix=$DIR/$prefix --with-ilmbase-prefix=$DIR/$prefix; make; make install;
    cd ..
fi

export OPENEXR_HOME=$DIR/$prefix

# ------------------------------------------------------------------------------ 
# Install Jpeg libary
# ------------------------------------------------------------------------------
if [[ "$param" == 'jpeg' ]] || [[ "$param" == 'all' ]]; then
    echo "Installing jpeg"
    cd jpeg-8c; ./configure --prefix=$DIR/$prefix; make; make install;
    cd ..
fi

# ------------------------------------------------------------------------------ 
# Install Tiff library
# ------------------------------------------------------------------------------
if [[ "$param" == 'tiff' ]] || [[ "$param" == 'all' ]]; then
    echo "Installing tiff"
    cd tiff-3.9.5; ./configure --prefix=$DIR/$prefix; make; make install;
    cd ..
fi

# ------------------------------------------------------------------------------ 
# Install PNG library
# ------------------------------------------------------------------------------
if [[ "$param" == 'png' ]] || [[ "$param" == 'all' ]]; then
    echo "Installing PNG"
    cd libpng; cmake -DCMAKE_INSTALL_PREFIX=$DIR/$prefix; make; make install;
    cd ..
fi



# ------------------------------------------------------------------------------ 
# Install Boost
# ------------------------------------------------------------------------------
if [[ "$param" == 'boost' ]] || [[ "$param" == 'all' ]]; then
    echo "Installing Boost"
    cd boost; ./bootstrap.sh --prefix=$DIR/$prefix; ./b2 --layout=tagged; ./b2 install --layout=tagged
    cd ..
fi

# Set environment variable BOOST_ROOT
export BOOST_ROOT=$DIR/$prefix

# ------------------------------------------------------------------------------
# Instal Open Image IO
# ------------------------------------------------------------------------------
if [[ "$param" == 'oiio' ]] || [[ "$param" == 'all' ]]; then
    echo "Installing oiio"
    cd oiio; make USE_TBB=0 INSTALLDIR=$DIR ;
    cd ..
fi

export OPENIMAGEIOHOME=$DIR/$prefix

# ------------------------------------------------------------------------------
# Install OSL
# ------------------------------------------------------------------------------
if [[ "$param" == 'osl' ]] || [[ "$param" == 'all' ]]; then
    cd osl; make INSTALLDIR=$DIR;
    cd ..
fi

export OSLHOME=$DIR/$prefix
