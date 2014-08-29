var async     = require("async");
var assert    = require('assert');
var Amount    = require("stellar-lib").Amount;
var Remote    = require("stellar-lib").Remote;
var Server    = require("./server").Server;
var Promise   = require('bluebird');
var testutils = require("./testutils");
var config    = testutils.init_config();

suite('Gateway', function() {
    var $ = { };

    setup(function(done) {
        testutils.build_setup().call($, done);
    });

    teardown(function(done) {
        testutils.build_teardown().call($, done);
    });

    test("subscribe test: customer to customer with and without transfer fee: transaction retry logic", function (done) {

        var self = this;

        // $.remote.set_trace();

        var steps = [
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
            },

            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote,
                    {
                        "alice" : "100/AUD/mtgox",
                        "bob"   : "100/AUD/mtgox",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote, { "mtgox" : [ "1/AUD/alice" ] },  callback);
            },

            function (callback) {
                self.what = "Verify balances.";

                testutils.verify_balances($.remote, {
                        "alice"   : "1/AUD/mtgox",
                        "mtgox"   : "-1/AUD/alice",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Alice sends Bob 1 AUD";

                $.remote.transaction()
                    .payment("alice", "bob", "1/AUD/mtgox")
                    .on('proposed', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) {
                self.what = "Verify balances 2.";

                testutils.verify_balances($.remote, {
                        "alice"   : "0/AUD/mtgox",
                        "bob"     : "1/AUD/mtgox",
                        "mtgox"   : "-1/AUD/bob",
                    },
                    callback);
            },

            //          function (callback) {
            //            self.what = "Set transfer rate.";
            //
            //            $.remote.transaction()
            //              .account_set("mtgox")
            //              .transfer_rate(1e9*1.1)
            //              .once('proposed', function (m) {
            //                  // console.log("proposed: %s", JSON.stringify(m));
            //                  callback(m.engine_result !== 'tesSUCCESS');
            //                })
            //              .submit();
            //          },

            function (callback) {
                self.what = "Bob sends Alice 0.5 AUD";

                $.remote.transaction()
                    .payment("bob", "alice", "0.5/AUD/mtgox")
                    .on('proposed', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) {
                self.what = "Verify balances 3.";

                testutils.verify_balances($.remote, {
                        "alice"   : "0.5/AUD/mtgox",
                        "bob"     : "0.5/AUD/mtgox",
                        "mtgox"   : [ "-0.5/AUD/alice","-0.5/AUD/bob" ],
                    },
                    callback);
            },

            function (callback) {
                self.what  = "Subscribe and accept.";
                self.count = 0;
                self.found = 0;

                testutils.rpc(config,'{"method":"ledger"}')
                    .then(function(result){
                        //console.log('ledger1 '+ JSON.stringify(result));
                        return testutils.rpc(config,'{"method":"ledger_accept"}'); })
                    .then(function(result){
                        //console.log('ledger_accept '+ JSON.stringify(result));
                        return testutils.rpc(config,'{"method":"ledger"}'); })
                    .then(function(result){
                        //console.log('ledger2 '+JSON.stringify(result));
                        callback();});

            },
            function (callback) {
                self.what = "Verify balances 4.";

                testutils.verify_balances($.remote, {
                        "alice"   : "0.5/AUD/mtgox",
                        "bob"     : "0.5/AUD/mtgox",
                        "mtgox"   : [ "-0.5/AUD/alice","-0.5/AUD/bob" ]
                    },
                    callback);
            },
        ]

        async.waterfall(steps, function (error) {
            assert(!error, self.what);
            done();
        });
    });

    test("trust line", function (done) {
        var self = this;

        var steps = [
            function (callback) {
                self.what = "Create accounts.";
                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox", "bitstamp", "amazon"], callback);
            },

            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote, {
                        "alice" : [ "100/BTC/amazon", "100/BTC/bitstamp", "100/BTC/mtgox", ] ,
                        "bob"   : "100/BTC/amazon",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote, {
                        "mtgox" 	 : [ "5/BTC/alice" ],
                        "bitstamp" : [ "5/BTC/alice" ],
                        "amazon" 	 : [ "5/BTC/alice" ],
                    },
                    callback);

            },

            function (callback) {
                self.what = "Verify balances.";

                testutils.verify_balances($.remote, {
                        "alice"   : ["5/BTC/mtgox", "5/BTC/bitstamp", "5/BTC/amazon", ],
                        "amazon"  : "-5/BTC/alice",
                    },
                    callback);

            },

            function (callback) {
                self.what = "Alice sends Bob 3 BTC";

                $.remote.transaction()
                    .payment("alice", "bob", "3/BTC/bob")
                    .build_path(true)
                    .once('proposed', function (m) {
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();

            },

            function (callback) {
                self.what = "Verify balances 2.";

                testutils.verify_balances($.remote, {
                        "alice"   : ["2/BTC/amazon", "5/BTC/mtgox", "5/BTC/bitstamp", ],
                        "bob"     : "3/BTC/amazon",
                        "amazon"   : ["-3/BTC/bob", "-2/BTC/alice"],
                    },
                    callback);

            },
        ]

        async.waterfall(steps, function(error) {
            assert(!error, self.what);
            done();
        });
    });



    test("customer to customer with and without transfer fee", function (done) {
        var self = this;

        // $.remote.set_trace();

        var steps = [
            function (callback) {
                self.what = "Create accounts.";
                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
            },

            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote, {
                        "alice" : "100/AUD/mtgox",
                        "bob"   : "100/AUD/mtgox",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote, {
                        "mtgox" : [ "1/AUD/alice" ],
                    },
                    callback);
            },

            function (callback) {
                self.what = "Verify balances.";

                testutils.verify_balances($.remote, {
                        "alice"   : "1/AUD/mtgox",
                        "mtgox"   : "-1/AUD/alice",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Alice sends Bob 1 AUD";

                $.remote.transaction()
                    .payment("alice", "bob", "1/AUD/mtgox")
                    .once('proposed', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) {
                self.what = "Verify balances 2.";

                testutils.verify_balances($.remote, {
                        "alice"   : "0/AUD/mtgox",
                        "bob"     : "1/AUD/mtgox",
                        "mtgox"   : "-1/AUD/bob",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Set transfer rate.";

                $.remote.transaction()
                    .account_set("mtgox")
                    .transfer_rate(1e9*1.1)
                    .once('proposed', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) {
                self.what = "Bob sends Alice 0.5 AUD";

                $.remote.transaction()
                    .payment("bob", "alice", "0.5/AUD/mtgox")
                    .send_max("0.55/AUD/mtgox") // !!! Very important.
                    .once('proposed', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) {
                self.what = "Verify balances 3.";

                testutils.verify_balances($.remote, {
                        "alice"   : "0.5/AUD/mtgox",
                        "bob"     : "0.45/AUD/mtgox",
                        "mtgox"   : [ "-0.5/AUD/alice","-0.45/AUD/bob" ],
                    },
                    callback);
            },
        ]

        async.waterfall(steps, function(error) {
            assert(!error, self.what);
            done();
        });
    });

    test("customer to customer, transfer fee, default path with and without specific issuer for Amount and SendMax", function (done) {

        var self = this;

        // $.remote.set_trace();

        var steps = [
            function (callback) {
                self.what = "Create accounts.";
                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
            },

            function (callback) {
                self.what = "Set transfer rate.";

                $.remote.transaction()
                    .account_set("mtgox")
                    .transfer_rate(1e9*1.1)
                    .once('submitted', function (m) {
                        // console.log("submitted: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote, {
                        "alice" : "100/AUD/mtgox",
                        "bob"   : "100/AUD/mtgox",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote, {
                        "mtgox" : [ "4.4/AUD/alice" ],
                    },
                    callback);
            },

            function (callback) {
                self.what = "Verify balances.";

                testutils.verify_balances($.remote, {
                        "alice"   : "4.4/AUD/mtgox",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Alice sends 1.1/AUD/mtgox Bob 1/AUD/mtgox";

                $.remote.transaction()
                    .payment("alice", "bob", "1/AUD/mtgox")
                    .send_max("1.1/AUD/mtgox")
                    .once('submitted', function (m) {
                        // console.log("submitted: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) {
                self.what = "Verify balances 2.";

                testutils.verify_balances($.remote, {
                        "alice"   : "3.3/AUD/mtgox",
                        "bob"     : "1/AUD/mtgox",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Alice sends 1.1/AUD/mtgox Bob 1/AUD/bob";

                $.remote.transaction()
                    .payment("alice", "bob", "1/AUD/bob")
                    .send_max("1.1/AUD/mtgox")
                    .once('submitted', function (m) {
                        // console.log("submitted: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) {
                self.what = "Verify balances 3.";

                testutils.verify_balances($.remote, {
                        "alice"   : "2.2/AUD/mtgox",
                        "bob"     : "2/AUD/mtgox",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Alice sends 1.1/AUD/alice Bob 1/AUD/mtgox";

                $.remote.transaction()
                    .payment("alice", "bob", "1/AUD/mtgox")
                    .send_max("1.1/AUD/alice")
                    .once('submitted', function (m) {
                        // console.log("submitted: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) {
                self.what = "Verify balances 4.";

                testutils.verify_balances($.remote, {
                        "alice"   : "1.1/AUD/mtgox",
                        "bob"     : "3/AUD/mtgox",
                    },
                    callback);
            },

            function (callback) {
                // Must fail, doesn't know to use the mtgox
                self.what = "Alice sends 1.1/AUD/alice Bob 1/AUD/bob";

                $.remote.transaction()
                    .payment("alice", "bob", "1/AUD/bob")
                    .send_max("1.1/AUD/alice")
                    .once('submitted', function (m) {
                        // console.log("submitted: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tecPATH_DRY');
                    })
                    .submit();
            },

            function (callback) {
                self.what = "Verify balances 5.";

                testutils.verify_balances($.remote, {
                        "alice"   : "1.1/AUD/mtgox",
                        "bob"     : "3/AUD/mtgox",
                    },
                    callback);
            }
        ]

        async.waterfall(steps, function(error) {
            assert(!error, self.what);
            done();
        });
    });

    test("subscribe test customer to customer with and without transfer fee", function (done) {
        var self = this;

        // $.remote.set_trace();

        var steps = [
            function (callback) {
                self.what = "Create accounts.";
                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
            },

            function (callback) { testutils.ledger_close($.remote, callback); },

            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote, {
                        "alice" : "100/AUD/mtgox",
                        "bob"   : "100/AUD/mtgox",
                    },
                    callback);
            },

            function (callback) { testutils.ledger_close($.remote, callback); },

            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote, {
                        "mtgox" : [ "1/AUD/alice" ],
                    },
                    callback);
            },

            function (callback) { testutils.ledger_close($.remote, callback); },

            function (callback) {
                self.what = "Verify balances.";

                testutils.verify_balances($.remote, {
                        "alice"   : "1/AUD/mtgox",
                        "mtgox"   : "-1/AUD/alice",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Alice sends Bob 1 AUD";

                $.remote.transaction()
                    .payment("alice", "bob", "1/AUD/mtgox")
                    .on('proposed', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) { testutils.ledger_close($.remote, callback); },

            function (callback) {
                self.what = "Verify balances 2.";

                testutils.verify_balances($.remote, {
                        "alice"   : "0/AUD/mtgox",
                        "bob"     : "1/AUD/mtgox",
                        "mtgox"   : "-1/AUD/bob",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Set transfer rate.";

                $.remote.transaction()
                    .account_set("mtgox")
                    .transfer_rate(1e9*1.1)
                    .once('proposed', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) { testutils.ledger_close($.remote, callback); },

            function (callback) {
                self.what = "Bob sends Alice 0.5 AUD";

                $.remote.transaction()
                    .payment("bob", "alice", "0.5/AUD/mtgox")
                    .send_max("0.55/AUD/mtgox") // !!! Very important.
                    .on('proposed', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },

            function (callback) {
                self.what = "Verify balances 3.";

                testutils.verify_balances($.remote, {
                        "alice"   : "0.5/AUD/mtgox",
                        "bob"     : "0.45/AUD/mtgox",
                        "mtgox"   : [ "-0.5/AUD/alice","-0.45/AUD/bob" ],
                    },
                    callback);
            },

            function (callback) {
                self.what  = "Subscribe and accept.";
                self.count = 0;
                self.found = 0;

                $.remote
                    .on('transaction', function (m) {
                        // console.log("ACCOUNT: %s", JSON.stringify(m));
                        self.found = 1;
                    })
                    .on('ledger_closed', function (m) {
                        // console.log("LEDGER_CLOSE: %d: %s", self.count, JSON.stringify(m));
                        if (self.count) {
                            callback(!self.found);
                        } else {
                            self.count  = 1;
                            $.remote.ledger_accept();
                        }
                    })
                    .request_subscribe().accounts("mtgox")
                    .request();

                $.remote.ledger_accept();
            }
        ]

        async.waterfall(steps, function(error) {
            assert(!error, self.what);
            done();
        });
    });


});