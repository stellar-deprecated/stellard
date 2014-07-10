VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  config.vm.box = "ubuntu/trusty64"
  config.ssh.forward_agent = true

  config.vm.network "forwarded_port", guest: 9001, host: 9001
  config.vm.network "forwarded_port", guest: 9002, host: 9002

  config.vm.provision "shell", inline: <<-EOS
    add-apt-repository -y ppa:boost-latest/ppa 
    apt-get update
    apt-get -y upgrade
    apt-get -y install git scons ctags pkg-config protobuf-compiler libprotobuf-dev libssl-dev python-software-properties libboost1.55-all-dev nodejs
    cd /stellard-src
    scons

    # setup data dir
    mkdir -p /var/lib/stellard

    # add helper script
    ln -nfs /stellard-src/vagrant/stellar /usr/local/bin/stellar
    chmod a+x /usr/local/bin/stellar

    # start the new ledger
    echo "starting new ledger"
    (stellar 2>/dev/null &)
    sleep 10
    pgrep stellard | xargs kill -INT

    # start service
    ln -nfs /stellard-src/vagrant/upstart.conf /etc/init/stellard.conf
    initctl reload-configuration
    service stellard start
  EOS

  config.vm.synced_folder "./", "/stellard-src"

  config.vm.provider "virtualbox" do |vb|
    vb.customize ["modifyvm", :id, "--memory", "4096"]
  end
end
