var async       = require("async");
var assert      = require('assert');
var Amount      = require("stellar-lib").Amount;
var Remote      = require("stellar-lib").Remote;
var Transaction = require("stellar-lib").Transaction;
var Server      = require("./server").Server;
var testutils   = require("./testutils");
var config      = testutils.init_config();


suite('Indirect paths', function() {
    var $ = { };

    setup(function(done) {
        testutils.build_setup().call($, done);
    });

    teardown(function(done) {
        testutils.build_teardown().call($, done);
    });

    test("path find", function (done) {
        var self = this;

        async.waterfall([
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol"], callback);
            },
            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote,
                    {
                        "bob"   : "1000/USD/alice",
                        "carol" : "2000/USD/bob",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Find path from alice to carol";

                $.remote.request_ripple_path_find("alice", "carol", "5/USD/carol",
                    [ { 'currency' : "USD" } ])
                    .on('success', function (m) {
                        //console.log("proposed: %s", JSON.stringify(m));

                        // 1 alternative.
                        assert.strictEqual(1, m.alternatives.length);

                        assert.strictEqual(1, m.alternatives[0].paths_computed.length);

                        callback();
                    })
                    .request();
            } ], function (error) {
            assert(!error, self.what);
            done();
        });
    });
});

suite('Quality paths', function() {
    var $ = { };

    setup(function(done) {
        testutils.build_setup().call($, done);
    });

    teardown(function(done) {
        testutils.build_teardown().call($, done);
    });

    test("quality set and test", function (done) {
        var self = this;

        async.waterfall([
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
            },
            function (callback) {
                self.what = "Set credit limits extended.";

                testutils.credit_limits($.remote,
                    {
                        "bob"   : "1000/USD/alice:2000,1400000000",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Verify credit limits extended.";

                testutils.verify_limit($.remote, "bob", "1000/USD/alice:2000,1400000000", callback);
            },
        ], function (error) {
            assert(!error, self.what);
            done();
        });
    });

    test("// quality payment (BROKEN DUE TO ROUNDING)", function (done) {
        var self = this;

        return done();

        async.waterfall([
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
            },
            function (callback) {
                self.what = "Set credit limits extended.";

                testutils.credit_limits($.remote,
                    {
                        "bob"   : "1000/USD/alice:" + .9*1e9 + "," + 1e9,
                    },
                    callback);
            },
            function (callback) {
                self.what = "Payment with auto path";

                $.remote.trace = true;

                $.remote.transaction()
                    .payment('alice', 'bob', "100/USD/bob")
                    .send_max("120/USD/alice")
//              .set_flags('PartialPayment')
//              .build_path(true)
                    .once('submitted', function (m) {
                        //console.log("proposed: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },
            function (callback) {
                self.what = "Display ledger";

                $.remote.request_ledger('current', { accounts: true, expand: true })
                    .on('success', function (m) {
                        //console.log("Ledger: %s", JSON.stringify(m, undefined, 2));

                        callback();
                    })
                    .request();
            },
        ], function (error) {
            assert(!error, self.what);
            done();
        });
    });
});

suite("Trust auto clear", function() {
    var $ = { };

    setup(function(done) {
        testutils.build_setup().call($, done);
    });

    teardown(function(done) {
        testutils.build_teardown().call($, done);
    });

    test("trust normal clear", function (done) {
        var self = this;

        async.waterfall([
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
            },
            function (callback) {
                self.what = "Set credit limits.";

                // Mutual trust.
                testutils.credit_limits($.remote,
                    {
                        "alice"   : "1000/USD/bob",
                        "bob"   : "1000/USD/alice",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Verify credit limits.";

                testutils.verify_limit($.remote, "bob", "1000/USD/alice", callback);
            },
            function (callback) {
                self.what = "Clear credit limits.";

                // Mutual trust.
                testutils.credit_limits($.remote,
                    {
                        "alice"   : "0/USD/bob",
                        "bob"   : "0/USD/alice",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Verify credit limits.";

                testutils.verify_limit($.remote, "bob", "0/USD/alice", function (m) {
                    var success = m && 'remoteError' === m.error && 'entryNotFound' === m.remote.error;

                    callback(!success);
                });
            },
            // YYY Could verify owner counts are zero.
        ], function (error) {
            assert(!error, self.what);
            done();
        });
    });

    test("trust auto clear", function (done) {
        var self = this;

        async.waterfall([
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
            },
            function (callback) {
                self.what = "Set credit limits.";

                // Mutual trust.
                testutils.credit_limits($.remote,
                    {
                        "alice" : "1000/USD/bob",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote,
                    {
                        "bob" : [ "50/USD/alice" ],
                    },
                    callback);
            },
            function (callback) {
                self.what = "Clear credit limits.";

                // Mutual trust.
                testutils.credit_limits($.remote,
                    {
                        "alice"   : "0/USD/bob",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Verify credit limits.";

                testutils.verify_limit($.remote, "alice", "0/USD/bob", callback);
            },
            function (callback) {
                self.what = "Return funds.";

                testutils.payments($.remote,
                    {
                        "alice" : [ "50/USD/bob" ],
                    },
                    callback);
            },
            function (callback) {
                self.what = "Verify credit limit gone.";

                testutils.verify_limit($.remote, "bob", "0/USD/alice", function (m) {
                    var success = m && 'remoteError' === m.error && 'entryNotFound' === m.remote.error;

                    callback(!success);
                });
            },
        ], function (error) {
            assert(!error, self.what);
            done();
        });
    });
});