var async     = require("async");
var assert    = require('assert');
var Amount    = require("stellar-lib").Amount;
var Remote    = require("stellar-lib").Remote;
var Server    = require("./server").Server;
var Promise   = require('bluebird');
var testutils = require("./testutils");
var config    = testutils.init_config();


suite('Indirect ripple', function() {
    var $ = { };

    setup(function(done) {
        testutils.build_setup().call($, done);
    });

    teardown(function(done) {
        testutils.build_teardown().call($, done);
    });

    test("too many path steps", function (done) {
        var self = this;


        var steps = [
            function (callback) {

                self.what = "Create accounts.";
                testutils.create_accounts($.remote, "root", "10000.0", ["gnnVq3ghZ3xoRE9tG78zCh5AWodVmEey32"], callback);
            },

            function (callback) {


                self.what = "Alice pays root via long path";

                var tx='{ "method": "submit",                                           \
                    "params": [                                                         \
                    { "secret": "s3q5ZGX2ScQK2rJ4JATp7rND6X5npG3De8jMbB7tuvm2HAVHcCN",  \
                        "tx_json": {                                                    \
                            "TransactionType": "Payment",                               \
                            "Account" : "ganVp9o5emfzpwrG5QVUXqMv8AgLcdvySb",           \
                            "Amount" : {\
                                "currency" : "USD",\
                                "issuer" : "ghj4kXtHfQcCaLQwpLJ11q2hq6248R7k9C", \
                                "value" : "1"\
                                },\
                            "Destination" : "gnnVq3ghZ3xoRE9tG78zCh5AWodVmEey32", \
                            "Fee" : "10",\
                            "Flags" : 2147483648,\
                            "Paths" : [[\
                                {\
                                    "currency" : "USD",\
                                    "issuer" : "ghj4kXtHfQcCaLQwpLJ11q2hq6248R7k9C" \
                                },\
                                {\
                                    "account" : "ghj4kXtHfQcCaLQwpLJ11q2hq6248R7k9C", \
                                    "type" : 1\
                                },\
                                {\
                                    "account" : "ghj4kXtHfQcCaLQwpLJ11q2hq6248R7k9C", \
                                    "type" : 1\
                                },\
                                {\
                                    "account" : "ghj4kXtHfQcCaLQwpLJ11q2hq6248R7k9C", \
                                    "type" : 1\
                                },\
                                {\
                                    "account" : "ghj4kXtHfQcCaLQwpLJ11q2hq6248R7k9C", \
                                    "type" : 1\
                                },\
                                {\
                                    "account" : "ghj4kXtHfQcCaLQwpLJ11q2hq6248R7k9C", \
                                    "type" : 1\
                                },\
                                {\
                                    "account" : "ghj4kXtHfQcCaLQwpLJ11q2hq6248R7k9C", \
                                    "type" : 1\
                                },\
                                {\
                                    "account" : "ghj4kXtHfQcCaLQwpLJ11q2hq6248R7k9C", \
                                    "type" : 1\
                                },\
                                {\
                                    "account" : "ghj4kXtHfQcCaLQwpLJ11q2hq6248R7k9C", \
                                    "type" : 1\
                                },\
                                {\
                                    "account" : "ghj4kXtHfQcCaLQwpLJ11q2hq6248R7k9C", \
                                    "type" : 1\
                                }\
                                ]] } } ] }';

                //console.log(tx);

                testutils.rpc(config,tx).then(function(result){
                    if(result.engine_result!='telBAD_PATH_COUNT')
                    {
                        console.log(JSON.stringify(result));
                        assert(0);
                    }
                    callback(result.engine_result!=='telBAD_PATH_COUNT')

                });
            }
            ];


        async.waterfall(steps, function (error) {
            assert(!error, self.what);
            done();
        });
    });

    test("indirect ripple", function (done) {
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
                        "alice" : "600/USD/mtgox",
                        "bob"   : "700/USD/mtgox",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote, {
                        "mtgox" : [ "70/USD/alice", "50/USD/bob" ],
                    },
                    callback);
            },
            function (callback) {
                self.what = "Verify alice balance with mtgox.";

                testutils.verify_balance($.remote, "alice", "70/USD/mtgox", callback);
            },
            function (callback) {
                self.what = "Verify bob balance with mtgox.";

                testutils.verify_balance($.remote, "bob", "50/USD/mtgox", callback);
            },
            function (callback) {
                self.what = "Alice sends more than has to issuer: 100 out of 70";

                $.remote.transaction()
                    .payment("alice", "mtgox", "100/USD/mtgox")
                    .once('submitted', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tecPATH_PARTIAL');
                    })
                    .submit();
            },
            function (callback) {
                self.what = "Alice sends more than has to bob: 100 out of 70";

                $.remote.transaction()
                    .payment("alice", "bob", "100/USD/mtgox")
                    .once('submitted', function (m) {
                        //console.log("proposed: %s", JSON.stringify(m));
                        callback(m.engine_result !== 'tecPATH_PARTIAL');
                    })
                    .submit();
            }
        ]

        async.waterfall(steps, function(error) {
            assert(!error, self.what);
            done();
        });
    });

    test("indirect ripple with path", function (done) {
        var self = this;

        var steps = [
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
            },
            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote, {
                        "alice" : "600/USD/mtgox",
                        "bob"   : "700/USD/mtgox",
                    },
                    callback);
            },
            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote, {
                        "mtgox" : [ "70/USD/alice", "50/USD/bob" ],
                    },
                    callback);
            },
            function (callback) {
                self.what = "Alice sends via a path";

                $.remote.transaction()
                    .payment("alice", "bob", "5/USD/mtgox")
                    .path_add( [ { account: "mtgox" } ])
                    .on('proposed', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },
            function (callback) {
                self.what = "Verify alice balance with mtgox.";

                testutils.verify_balance($.remote, "alice", "65/USD/mtgox", callback);
            },
            function (callback) {
                self.what = "Verify bob balance with mtgox.";

                testutils.verify_balance($.remote, "bob", "55/USD/mtgox", callback);
            }
        ]

        async.waterfall(steps, function(error) {
            assert(!error, self.what);
            done();
        });
    });




    test("indirect ripple with multi path", function (done) {
        var self = this;

        var steps = [
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "amazon", "mtgox"], callback);
            },
            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote, {
                        "amazon"  : "2000/USD/mtgox",
                        "bob"   : [ "600/USD/alice", "1000/USD/mtgox" ],
                        "carol" : [ "700/USD/alice", "1000/USD/mtgox" ],
                    },
                    callback);
            },
            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote, {
                        "mtgox" : [ "100/USD/bob", "100/USD/carol" ],
                    },
                    callback);
            },
            function (callback) {
                self.what = "Alice pays amazon via multiple paths";

                $.remote.transaction()
                    .payment("alice", "amazon", "150/USD/mtgox")
                    .path_add( [ { account: "bob" } ])
                    .path_add( [ { account: "carol" } ])
                    .on('proposed', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));
                        if(m.engine_result !== 'tesSUCCESS') console.log(JSON.stringify(m));
                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },
            function (callback) {
                self.what = "Verify balances.";

                testutils.verify_balances($.remote, {
                        "alice"   : [ "-100/USD/bob", "-50/USD/carol" ],
                        "amazon"  : "150/USD/mtgox",
                        "bob"     : "0/USD/mtgox",
                        "carol"   : "50/USD/mtgox",
                    },
                    callback);
            },
        ]

        async.waterfall(steps, function(error) {
            assert(!error, self.what);
            done();
        });
    });

    test("indirect ripple with path and transfer fee", function (done) {
        var self = this;

        var steps = [
            function (callback) {
                self.what = "Create accounts.";

                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "amazon", "mtgox"], callback);
            },
            function (callback) {
                self.what = "Set mtgox transfer rate.";

                testutils.transfer_rate($.remote, "mtgox", 1.1e9, callback);
            },
            function (callback) {
                self.what = "Set credit limits.";

                testutils.credit_limits($.remote, {
                        "amazon"  : "2000/USD/mtgox",
                        "bob"   : [ "600/USD/alice", "1000/USD/mtgox" ],
                        "carol" : [ "700/USD/alice", "1000/USD/mtgox" ],
                    },
                    callback);
            },
            function (callback) {
                self.what = "Distribute funds.";

                testutils.payments($.remote, {
                        "mtgox" : [ "100/USD/bob", "100/USD/carol" ],
                    },
                    callback);
            },
            function (callback) {
                self.what = "Alice pays amazon via multiple paths";

                $.remote.transaction()
                    .payment("alice", "amazon", "150/USD/mtgox")
                    .send_max("200/USD/alice")
                    .path_add( [ { account: "bob" } ])
                    .path_add( [ { account: "carol" } ])
                    .on('proposed', function (m) {
                        // console.log("proposed: %s", JSON.stringify(m));

                        callback(m.engine_result !== 'tesSUCCESS');
                    })
                    .submit();
            },
            //          function (callback) {
            //            self.what = "Display ledger";
            //
            //            $.remote.request_ledger('current', true)
            //              .on('success', function (m) {
            //                  console.log("Ledger: %s", JSON.stringify(m, undefined, 2));
            //
            //                  callback();
            //                })
            //              .request();
            //          },
            function (callback) {
                self.what = "Verify balances.";

                // 65.00000000000001 is correct.
                // This is result of limited precision.
                testutils.verify_balances($.remote, {
                        "alice"   : [ "-100/USD/bob", "-65.00000000000001/USD/carol" ],
                        "amazon"  : "150/USD/mtgox",
                        "bob"     : "0/USD/mtgox",
                        "carol"   : "35/USD/mtgox",
                    },
                    callback);
            }
        ]

        async.waterfall(steps, function(error) {
            assert(!error, self.what);
            done();
        });
    })
});