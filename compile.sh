# ------------------------------------------------------------------------------
# This script compiles osl and its dependencies 
#
# Assuming installed libs:
#    * LLVM (only tested with 2.9)
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


# ------------------------------------------------------------------------------
# Variables
# ------------------------------------------------------------------------------
prefix=dist/

# Dir where this script is stored
DIR="$( cd "$( dirname "$0" )" && pwd )"
# Dir where we'll install everything
echo $DIR/$prefix

# ------------------------------------------------------------------------------
# Install ilmbase
# ------------------------------------------------------------------------------
echo "Installing ilmbase"
cd ilmbase; ./configure --prefix=$DIR/$prefix; make; make install
cd ..

export ILMBASE_HOME=$DIR/$prefix

# ------------------------------------------------------------------------------
# Install openexr
# ------------------------------------------------------------------------------
echo "Installing openexr"
cd openexr; ./configure --prefix=$DIR/$prefix --with-ilmbase-prefix=$DIR/$prefix; make;
cd ..

export OPENEXR_HOME=$DIR/$prefix

# ------------------------------------------------------------------------------
# Instal Open Image IO
# ------------------------------------------------------------------------------
echo "Installing oiio"
cd oiio; make USE_TBB=0 INSTALLDIR=$DIR/$prefix ;
cd ..

export OPENIMAGEIOHOME=$DIR/$prefix

# ------------------------------------------------------------------------------ 
# Install Boost
# ------------------------------------------------------------------------------
echo "Installing Boost"
cd boost; ./bootstrap.sh --prefix=$DIR/$prefix; ./b2 --layout=tagged; ./b2 install --layout=tagged
cd ..

# Set environment variable BOOST_ROOT
export BOOST_ROOT = $DIR/$prefix

# ------------------------------------------------------------------------------
# Install OSL
# ------------------------------------------------------------------------------
cd osl; make INSTALLDIR=$DIR/$prefix;
cd ..

export OSLHOME=$DIR/$prefix
