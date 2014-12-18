Overview
========

These are the instructions for building [stellard](https://github.com/stellar/stellard) and running unit tests. Binaries for [stellard](https://github.com/stellar/stellard) are not currently provided.

These instructions are provided as a guide. As anyone can edit this
wiki, for security reasons alone, you should make sure you understand
all commands and their implications.

stellard is built with a software construction tool named scons. The
build process checks that key packages - OpenSSL and Boost - are
up-to-date; you can also override compilation flag defaults, or set tags
to control aspects of the build like debug mode.

See also:
- [OSX build instructions](osx-build-instructions.md)

External dependencies
=====================

Your system should have the following development libraries installed.
Be sure to upgrade gcc to v4.8 or later because the codebase makes use
of constructs only supported in the latest releases.

boost
-----
-   <http://www.boost.org>

The latest version of boost is typically required to build stellard. We
suggest a separate installation of boost just for stellard, following
the build instructions for boost provided at <http://boost.org>.

OpenSSL
-------

OpenSSL is a security critical dependency for stellard. It is important
that you have an up-to-date version of this library.

Under Linux, you can install a fresh version like this:

```
apt-get install libssl-dev
```

ctags (optional)
----------------

CTags allows editors, such as vim, to be able to jump to function
definitions.

-   <http://ctags.sourceforge.net>

You can prevent scons' default use of ctags by explicitly specifying
stellard as your build target:

```
scons build/stellard
```

node.js and npm (optional)
--------------------------
- <https://github.com/joyent/node/wiki/installation>

While not strictly necessary, having node.js will allow you to run the
unit tests and help ensure your system is operating properly.

On some distributions, npm is included in the node.js package.

protocol buffers
----------------
-   <http://code.google.com/p/protobuf>

Minimum version: 2.4.1

(needed just for compiling)

libsodium
---------

download the tarball:
<https://download.libsodium.org/libsodium/releases/LATEST.tar.gz>

```
wget https://download.libsodium.org/libsodium/releases/LATEST.tar.gz
tar xzf LATEST.tar.gz
cd libsodium-0.7.0/
./configure
make && make check && sudo make install
```

Other packages
--------------

-   scons
-   pkg-config

```
apt-get install pkg-config
```

Getting the Repository
======================

Clone the repository:

```
git clone https://github.com/stellar/stellard.git
```

The repository is now in the `stellard` directory.

Move into the `stellard` directory:

```
cd stellard
```

Switch to the master branch:

```
git checkout master
```

Alternative, use the develop branch:

```
git checkout develop
```

Or use the release branch:

```
git checkout release
```

Internal dependencies
=====================

These elements should be brought into the Stellar development tree.

npm
---

npm is the node package manager. You will need nodejs installed on your
system.

Stellar has the following npm dependencies and more:

[stellar-lib](https://npmjs.org/package/stellar-lib) :   JavaScript library for stellard. Required for unit tests.

[async](https://npmjs.org/package/async):   Higher-order functions and common patterns for asynchronous code

[buster](https://npmjs.org/package/buster):   Buster.JS JavaScript test framework

From the base of the source directory, install the node dependencies.

Install all of the stellard test suite node dependencies:

```
npm install
```

This command installs the required packages in the current directory
under: `node_modules`

The ``"should be installed with -g"`` should be a harmless warning.

Unit testing
============

Ensure you have a current version of Node.js.

To install NPM dependencies, run:

```
npm install
```

Configuration
-------------

Add `node_modules/buster/bin` to your path:

tcsh:

```
set path=( node_modules/buster/bin $path )
```

bash:

```
PATH=node_modules/buster/bin:$PATH
```

Create a test/config.js:

```
cp test/config-example.js test/config.js
```

Edit config.js as appropriate for your system.

Running CLI tests
-----------------

The test runner will create a `tmp` folder in the root of the repository, with an auto-generated `stellar.cfg` file, and a folder for the db backend's disk storage. The configuration is generated from the `servers` object exported from `stellard/test/config.js`. There is an example configuration file at `stellard/test/config-example.js` you can copy to create this. You must specify a db backend (`node_db` in your stellard cfg) to be able to run the tests.

Running a single test:

```
node test/module-test.js
```

Running all tests:

```
npm test
```

Or, if you have Buster.JS installed globally:

```
buster test
```

node-inspector
--------------

Node-inspector allows using a browser as a debugger.

Installation:

```
npm install node-inspector
```

Add to path.

Debugging:

```
inspector.js &
node --debug-brk --debug whatever.js 
```

Head to <http://0.0.0.0:8080/debug?port=5858> to find your app nicely stopped on the first line.

Building
========

Build everything:

```
scons
```

Build only stellard:

```
scons build/stellard
```

Build quietly:

```
scons -Q
```

Clean out object files (equivalent to running \`make clean\`):

```
scons -c
```

Build with specified number of simultaneous jobs (ideal for
multiprocessor systems):

```
scons -j _jobs_
```

On Linux, specify the number of jobs automatically:

```
setenv SCONSFLAGS "-j `grep -c processor /proc/cpuinfo`"
```

The compiled binary will be found here:

```
build/stellard
```

By default stellard is compiled with debugging symbols included. This
increases the size of the executable by an order of magnatude.

Unit Tests
==========

Run the built-in unit tests:

```
build/stellard --unittest
```

Run a specific built-in unit test:

```
build/stellard --unittest -- --run_test=Utils
```

Run the Javascript driven unit tests:

```
npm install
npm test
```

Running multiple instances
==========================

For each instance you which to run:

Create a directory for the instance.

```
mkdir alpha ; cd alpha
```

Create a configuration file specifying where the server should listen
at: [stellard.cfg](https://github.com/stellar/stellard/blob/master/doc/stellard-example.cfg)

Launch stellard as a local, stand alone server:

```
stellard -a &
```

Use stellard as a command line agent to send a command to a
server:

```
stellard stop
```

OS specific instructions
========================

Whatever the platform, we recommend a minimum of 8Gb ram for building
stellard.

Amazon Web Services
-------------------

### Ubuntu (12.04 LTS Precise Pangolin)

These instructions are a guide to the commands needed to set up
stellard. You should understand the commands and the available
options before executing them. Most of these commands should be executed as root.

Verify the version of Ubuntu:

```bash
$ cat /etc/issue
Ubuntu 12.04.1 LTS \n \l
```

Update the packages database:

```
# sudo apt-get update
```

Upgrade the system:

```
# sudo apt-get upgrade
```

get add-apt-repository via:

```
# sudo apt-get install python-software-properties
```

Add a more recent node repository:

```
# sudo add-apt-repository ppa:chris-lea/node.js
```

Add a more recent Boost C++ library repository:

```
# sudo add-apt-repository ppa:boost-latest/ppa
```

Add a more recent gcc 4.8 repository:

```
# sudo add-apt-repository ppa:ubuntu-toolchain-r/test
```

Update packages with new repositories added:

```
# sudo apt-get update
```

If you have a smaller instance, you will want to create and enable swap
space. Otherwise, your system may not have enough memory to compile
stellard.

Create a swap file:

```
# dd if=/dev/zero of=/swap bs=1024 count=1M
```

Format the swap file:

```
# mkswap /swap
```

Enable the swap file:

```
# swapon /swap
```

Enable swap on boot:

```
# echo "/swap swap swap sw 0 0" >> /etc/fstab
```

To verify swap is enabled:

```
# swapon -s
Filename                      Type            Size    Used    Priority
/swap                         file            1048572 0       -1
```

Install various dependencies:

```
# apt-get install pkg-config git scons ctags
# apt-get install libboost1.55-all-dev
# apt-get install protobuf-compiler libprotobuf-dev
# apt-get install nodejs
# apt-get install gcc-4.8 g++-4.8
```

To be able to switch between gcc 4.6 (Ubuntu 12.04 default) and 4.8
(needed to build stellard), add alternatives:

```
# sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.6 60 --slave /usr/bin/g++ g++ /usr/bin/g++-4.6 
# sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 40 --slave /usr/bin/g++ g++ /usr/bin/g++-4.8
```

We need to switch to gcc 4.8 before building stellard. You should choose
gcc 4.8 when prompted. You can run this same command later if you'd like
to switch your default gcc version back to 4.6:

```
# sudo update-alternatives --config gcc
```

Check to make sure you are set to gcc 4.8 by running. This should be g++
4.8 (or later):

```
# g++ --version
```

We need to install libsodium, which does not reside in a apt-get
package. Download and make by running:
```
# wget https://download.libsodium.org/libsodium/releases/LATEST.tar.gz
# tar xzf LATEST.tar.gz
# cd libsodium-0.6.1/
# apt-get install make
# ./configure
# make && make check && sudo make install
```


Finally we download the source code for [stellard](https://github.com/stellar/stellard) and compile as normal:

```
# cd ..
# git clone https://github.com/stellar/stellard.git
# cd stellard/
# git checkout master
# scons
# ./build/stellard --unittest
# sudo apt-get install npm
# npm test
```


### Ubuntu (14.04 LTS)

As root make a text file and add:
```
# add-apt-repository -y ppa:boost-latest/ppa ; sudo apt-get update ; apt-get -y upgrade ; 
# sudo apt-get -y install git scons ctags pkg-config protobuf-compiler libprotobuf-dev libssl-dev python-software-properties libboost1.55-all-dev nodejs; 
# git clone https://github.com/stellar/stellard.git ; cd stellard/ ; git checkout master ; 
# scons ; ./build/stellard --unittest ; sudo apt-get install npm; npm test
```


Make sure to chmod +x the file name and remove the \# sign (which is
added for readability on the wiki page) before running.

Arch Linux
----------

Build and install the AUR package:

-   [Stable version](https://aur.archlinux.org/packages/stellard/)
-   [Development version](https://aur.archlinux.org/packages/stellard-git/)

Review the configuration file located at `/etc/stellard/stellard.cfg` and
start the server:

```
# stellard --conf=/etc/stellard/stellard.cfg
```

OSX
---

See [OSX Build Instructions](osx-build-instructions.md)

Ubuntu LTS 12.04.1, 64-bit
--------------------------

See the Amazon instructions above.

FreeBSD
-------

Install the following ports:

```
devel/boost-libs
devel/protobuf
devel/scons
security/openssl
```

Build stellard by running `scons` from the main directory. You may
receive errors about `exuberant-ctags` not being found and warnings
about symbols with names beginning with `ILL_` being redefined. These
can be ignored.

Windows
-------

There are no build instructions for Windows platforms yet.
