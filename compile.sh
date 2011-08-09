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

# ------------------------------------------------------------------------------
# Install openexr
# ------------------------------------------------------------------------------
echo "Installing openexr"
cd openexr; ./configure --prefix=$DIR/$prefix --with-ilmbase-prefix=$DIR/$prefix; make;

#--with-ilmbase-prefix=/Users/kunigami/dev/brlcad-root/brlcad/osl/trunk/dist
