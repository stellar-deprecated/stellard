VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  config.vm.box = "ubuntu/trusty64"
  config.ssh.forward_agent = true

  config.vm.network "forwarded_port", guest: 9001, host: 9001
  config.vm.network "forwarded_port", guest: 9002, host: 9002
  config.vm.network "forwarded_port", guest: 9101, host: 9101
  config.vm.network "forwarded_port", guest: 9102, host: 9102

  config.vm.provision "shell", inline: <<-EOS
    add-apt-repository -y ppa:boost-latest/ppa 
    apt-get update
    apt-get -y upgrade
    apt-get -y install git scons ctags pkg-config protobuf-compiler libprotobuf-dev libssl-dev python-software-properties libboost1.55-all-dev nodejs

    # build libsodium
    wget https://download.libsodium.org/libsodium/releases/libsodium-0.6.0.tar.gz
    tar -xzvf libsodium-0.6.0.tar.gz
    cd libsodium-0.6.0
    ./configure && make && sudo make install

    # build stellard
    cd /stellard-src
    scons

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
    service stellard-private-ledger start
    service stellard-public-ledger start
  EOS

  config.vm.synced_folder "./", "/stellard-src"

  config.vm.provider "virtualbox" do |vb|
    vb.customize ["modifyvm", :id, "--memory", "4096"]
  end
end
