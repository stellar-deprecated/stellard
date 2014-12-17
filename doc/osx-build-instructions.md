Overview
========

These are the instructions for building stellard on Mac OS X. These
instructions have only been tested with Mac OS X version 10.9.4
(Mavericks).

Required Dependencies
---------------------

### Xcode Developer Tools

You will need the latest version Xcode developer tools. Follow these
instructions if you do not have the developer tools:
<http://stackoverflow.com/questions/9329243/xcode-4-4-and-later-install-command-line-tools>

### Homebrew

You will need homebrew to install some additional dependencies. If you
don't already have it, use this simple one liner to install:

```
ruby -e "$(curl -fsSL https://raw.github.com/Homebrew/homebrew/go/install)"
```

You will also need to add homebrew libs and binaries to your PATH. You
can add this line to \~/.bash\_profile:

```
export PATH="/usr/local/bin:/usr/local/sbin:~/bin:$PATH"
```

### Other Required Dependencies

Before installing other dependencies, run:

```
brew update
```

Then, to get protobuf241, run the following command:

```
brew tap homebrew/versions
```

Then:

```
brew install boost openssl protobuf241 libsodium scons pkg-config
```

Link to protobuf manually by running:

```
brew link --force protobuf241
```

Because of recent security concerns, it is highly recommended to use the
latest version of openssl. If you previously installed openssl with
homebrew, you should run:

```
brew upgrade openssl 
```

You can tell your system to use the latest version by running:

```
brew link --force openssl
```

Installing boost through homebrew may result in a library called
“libboost\_thread-mt”. However during the build process, stellard does
not expect the "-mt" suffix. This is just a naming convention boost uses
for mac os x and the libraries function the same way. To let the
stellard build process find the boost libraries, symlink them with the
following two commands:

```
ln -s /usr/local/lib/libboost_thread-mt.a /usr/local/lib/libboost_thread.a
ln -s /usr/local/lib/libboost_thread-mt.dylib /usr/local/lib/libboost_thread.dylib
```

Optional Dependencies
---------------------

Assuming you followed the instructions for required dependencies above,
you can optionally install:

-   node.js and npm for certain unit tests
-   ctags for letting certain text editors jump to function definitions

To install node.js and npm, simply run:

```
brew install node
```

To install ctags:

```
brew install ctags
```

Building
--------

After you have installed all the dependencies by following the
instructions above, you may continue by following the general
instructions for [building stellard](building-stellard.md).
