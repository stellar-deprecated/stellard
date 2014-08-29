var async       = require("async");
var assert      = require('assert');
var Amount      = require("stellar-lib").Amount;
var Remote      = require("stellar-lib").Remote;
var Transaction = require("stellar-lib").Transaction;
var Server      = require("./server").Server;
var testutils   = require("./testutils");
var config      = testutils.init_config();


suite('Basic path finding', function() {
    var $ = { };

    setup(function(done) {
        testutils.build_setup().call($, done);
    });

    teardown(function(done) {
        testutils.build_teardown().call($, done);
    });

    test("no direct path, no intermediary -> no alternatives", function (done) {
        var self = this;

        var steps = [
            function (callback) {
                self.what = "Create accounts.";
                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
            },

            function (callback) {
                self.what = "Find path from alice to bob";
                var request = $.remote.request_ripple_path_find("alice", "bob", "5/USD/bob", [ { 'currency' : "USD" } ]);
                request.callback(function(err, m) {
                    if (m.alternatives.length) {
                        callback(new Error(m));
                    } else {
                        callback(null);
                    }
                });
            }
        ]

        async.waterfall(steps, function (error) {
            assert(!error, self.what);
            done();
        });
    });

    test("direct path, no intermediary", function (done) {
        var self = this;

        var steps = [
            function (callback) {
                self.what = "Create accounts.";
                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
            },
            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote, {
                        "bob"   : "700/USD/alice",
                    },
                    callback);
            },

            function (callback) {
                self.what = "Find path from alice to bob";
                $.remote.request_ripple_path_find("alice", "bob", "5/USD/bob",
                    [ { 'currency' : "USD" } ])
                    .on('success', function (m) {
                        //console.log("proposed: %s", JSON.stringify(m));

                        // 1 alternative.
                        assert.strictEqual(1, m.alternatives.length)
                        // Path is empty.
                        assert.strictEqual(0, m.alternatives[0].paths_computed.length)

                        callback();
                    })
                    .request();
            },
        ]

        async.waterfall(steps, function(error) {
            assert(!error);
            done();
        });
    });

    test("payment auto path find (using build_path)", function (done) {
        var self = this;

        var steps = [
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
            },
            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote,
                    {
                        "alice" : "600/USD/mtgox",
                        "bob"   : "700/USD/mtgox",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote,
                    {
                        "mtgox" : [ "70/USD/alice" ],
                    },
                    callback);
            },
            function (callback) {
                self.what = "Payment with auto path";

                $.remote.transaction()
                    .payment('alice', 'bob', "24/USD/bob")
                    .build_path(true)
                    .once('submitted', function (m) {
                        //console.log("proposed: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },
            function (callback) {
                self.what = "Verify balances.";

                testutils.verify_balances($.remote,
                    {
                        "alice"   : "46/USD/mtgox",
                        "mtgox"   : [ "-46/USD/alice", "-24/USD/bob" ],
                        "bob"     : "24/USD/mtgox",
                    },
                    callback);
            },
        ]

        async.waterfall(steps, function(error) {
            assert(!error, self.what);
            done();
        });
    });

    test("path find", function (done) {
        var self = this;

        var steps = [
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
            },
            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote,
                    {
                        "alice" : "600/USD/mtgox",
                        "bob"   : "700/USD/mtgox",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote,
                    {
                        "mtgox" : [ "70/USD/alice", "50/USD/bob" ],
                    },
                    callback);
            },
            function (callback) {
                self.what = "Find path from alice to mtgox";



                $.remote.request_ripple_path_find("alice", "bob", "5/USD/mtgox",
                    [ { 'currency' : "USD" } ])
                    .on('success', function (m) {
                        //console.log("proposed: %s", JSON.stringify(m));

                        // 1 alternative.
                        assert.strictEqual(1, m.alternatives.length);
                        assert.strictEqual(1, m.alternatives[0].paths_computed.length);
                        //assert.strictEqual(1, m.alternatives[0].paths_computed[0].type);

                        callback();
                    })
                    .request();
            }
        ]

        async.waterfall(steps, function (error) {
            assert(!error, self.what);
            done();
        });
    });



    test("alternative paths - consume both", function (done) {
        var self = this;

        async.waterfall([
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox", "bitstamp"], callback);
            },
            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote,
                    {
                        "alice" : [ "600/USD/mtgox", "800/USD/bitstamp" ],
                        "bob"   : [ "700/USD/mtgox", "900/USD/bitstamp" ]
                    },
                    callback);
            },
            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote,
                    {
                        "bitstamp" : "70/USD/alice",
                        "mtgox" : "70/USD/alice",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Payment with auto path";

                $.remote.transaction()
                    .payment('alice', 'bob', "140/USD/bob")
                    .build_path(true)
                    .once('submitted', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },
            function (callback) {
                self.what = "Verify balances.";

                testutils.verify_balances($.remote,
                    {
                        "alice"     : [ "0/USD/mtgox", "0/USD/bitstamp" ],
                        "bob"       : [ "70/USD/mtgox", "70/USD/bitstamp" ],
                        "bitstamp"  : [ "0/USD/alice", "-70/USD/bob" ],
                        "mtgox"     : [ "0/USD/alice", "-70/USD/bob" ],
                    },
                    callback);
            },
        ], function (error) {
            assert(!error, self.what);
            done();
        });
    });

    test("alternative paths - consume best transfer", function (done) {
        var self = this;

        async.waterfall([
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox", "bitstamp"], callback);
            },
            function (callback) {
                self.what = "Set transfer rate.";

                $.remote.transaction()
                    .account_set("bitstamp")
                    .transfer_rate(1e9*1.1)
                    .once('submitted', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },
            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote,
                    {
                        "alice" : [ "600/USD/mtgox", "800/USD/bitstamp" ],
                        "bob"   : [ "700/USD/mtgox", "900/USD/bitstamp" ]
                    },
                    callback);
            },
            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote,
                    {
                        "bitstamp" : "70/USD/alice",
                        "mtgox" : "70/USD/alice",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Payment with auto path";

                $.remote.transaction()
                    .payment('alice', 'bob', "70/USD/bob")
                    .build_path(true)
                    .once('submitted', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },
            function (callback) {
                self.what = "Verify balances.";

                testutils.verify_balances($.remote,
                    {
                        "alice"     : [ "0/USD/mtgox", "70/USD/bitstamp" ],
                        "bob"       : [ "70/USD/mtgox", "0/USD/bitstamp" ],
                        "bitstamp"  : [ "-70/USD/alice", "0/USD/bob" ],
                        "mtgox"     : [ "0/USD/alice", "-70/USD/bob" ],
                    },
                    callback);
            },
        ], function (error) {
            assert(!error, self.what);
            done();
        });
    });

    test("alternative paths - consume best transfer first", function (done) {
        var self = this;

        async.waterfall([
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox", "bitstamp"], callback);
            },
            function (callback) {
                self.what = "Set transfer rate.";

                $.remote.transaction()
                    .account_set("bitstamp")
                    .transfer_rate(1e9*1.1)
                    .once('submitted', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },
            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote,
                    {
                        "alice" : [ "600/USD/mtgox", "800/USD/bitstamp" ],
                        "bob"   : [ "700/USD/mtgox", "900/USD/bitstamp" ]
                    },
                    callback);
            },
            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote,
                    {
                        "bitstamp" : "70/USD/alice",
                        "mtgox" : "70/USD/alice",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Payment with auto path";

                $.remote.transaction()
                    .payment('alice', 'bob', "77/USD/bob")
                    .build_path(true)
                    .send_max("100/USD/alice")
                    .once('submitted', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },
            function (callback) {
                self.what = "Verify balances.";

                testutils.verify_balances($.remote,
                    {
                        "alice"     : [ "0/USD/mtgox", "62.3/USD/bitstamp" ],
                        "bob"       : [ "70/USD/mtgox", "7/USD/bitstamp" ],
                        "bitstamp"  : [ "-62.3/USD/alice", "-7/USD/bob" ],
                        "mtgox"     : [ "0/USD/alice", "-70/USD/bob" ],
                    },
                    callback);
            },
        ], function (error) {
            assert(!error, self.what);
            done();
        });
    });
});