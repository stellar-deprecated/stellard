FROM ubuntu:14.04
MAINTAINER Daniel Watkins <daniel@daniel-watkins.co.uk>

RUN apt-get update
RUN apt-get -y upgrade
RUN apt-get -y install git scons ctags pkg-config protobuf-compiler libprotobuf-dev libssl-dev python-software-properties libboost1.55-all-dev nodejs build-essential npm

# build libsodium
ADD https://download.libsodium.org/libsodium/releases/libsodium-1.0.0.tar.gz libsodium-1.0.0.tar.gz
RUN tar zxf libsodium-1.0.0.tar.gz && cd libsodium-1.0.0 && ./configure && make && make install

# build stellard
ADD . /stellard-src
RUN cd /stellard-src && scons && npm install

# setup data dir
RUN mkdir -p /var/lib/stellard

# add helper script
RUN ln -nfs /stellard-src/vagrant/stellar-private-ledger /usr/local/bin/stellar-private-ledger
RUN chmod a+x /stellard-src/vagrant/stellar-private-ledger
RUN ln -nfs /stellard-src/vagrant/stellar-public-ledger /usr/local/bin/stellar-public-ledger
RUN chmod a+x /stellard-src/vagrant/stellar-public-ledger

VOLUME "/var/lib/stellard"
EXPOSE 9101 9102 52101
CMD /usr/local/bin/stellar-public-ledger
