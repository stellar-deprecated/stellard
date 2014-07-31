VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  config.vm.box = "ubuntu/trusty64"
  config.ssh.forward_agent = true

  config.vm.network "forwarded_port", guest: 9001, host: 9001
  config.vm.network "forwarded_port", guest: 9002, host: 9002
  config.vm.network "forwarded_port", guest: 9101, host: 9101
  config.vm.network "forwarded_port", guest: 9102, host: 9102

  config.vm.provision "shell", inline: <<-EOS
    set -e

    add-apt-repository -y ppa:boost-latest/ppa
    apt-get update || true
    apt-get -y upgrade
    apt-get -y install git scons ctags pkg-config protobuf-compiler libprotobuf-dev libssl-dev python-software-properties libboost1.55-all-dev nodejs

    # build libsodium
    # This looks a bit funny, but we want to avoid touching any files if we've already
    # built this version of libsodium. And we need to make sure we don't behave badly
    # if we get killed in the middle of this.
    libsodium=libsodium-1.0.0
    if [[ ! -f $libsodium/.stellard.stamp ]]; then
        if ! wget -nv -O $libsodium.download https://download.libsodium.org/libsodium/releases/$libsodium.tar.gz; then
            # download failed?
            rm -f $libsodium.download
            exit 1
        fi
        mv -f $libsodium.download $libsodium.tar.gz
        tar -xzvf $libsodium.tar.gz
        # the stamp file says we finished untarring successfully
        touch $libsodium/.stellard.stamp
    fi
    cd $libsodium
    ./configure && make && sudo make install

    # build stellard
    cd /stellard-src
    scons

    # shut down any existing stellard upstart jobs
    initctl emit stellard-reprovision

    # setup data dir
    mkdir -p /var/lib/stellard

    # add helper script
    ln -nfs /stellard-src/vagrant/stellar-private-ledger /usr/local/bin/stellar-private-ledger
    chmod a+x /stellard-src/vagrant/stellar-private-ledger
    ln -nfs /stellard-src/vagrant/stellar-public-ledger /usr/local/bin/stellar-public-ledger
    chmod a+x /stellard-src/vagrant/stellar-public-ledger

    # start the new ledger
    echo "starting new ledger"
    (stellar-private-ledger --start --fg 2>/dev/null &)
    sleep 10
    pgrep stellard | xargs kill -INT

    # start service
    cp /stellard-src/vagrant/upstart-private-ledger.conf /etc/init/stellard-private-ledger.conf
    cp /stellard-src/vagrant/upstart-public-ledger.conf /etc/init/stellard-public-ledger.conf
    initctl reload-configuration
    initctl start stellard-private-ledger
    initctl start stellard-public-ledger
  EOS

  config.vm.synced_folder "./", "/stellard-src"

  config.vm.provider "virtualbox" do |vb|
    vb.customize ["modifyvm", :id, "--memory", "4096"]
  end
end
