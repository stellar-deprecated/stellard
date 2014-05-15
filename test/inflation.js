var async     = require("async");
var assert    = require('assert');
var Amount    = require("stellar-lib").Amount;
var Remote    = require("stellar-lib").Remote;
var Server    = require("./server").Server;
var testutils = require("./testutils");
var config    = testutils.init_config();

suite('Inflation', function() {
    var $ = { };

    setup(function(done) {
        testutils.build_setup().call($, done);
    });

    teardown(function(done) {
        testutils.build_teardown().call($, done);
    });

    test("Inflation to another account", function (done) {
        var self    = this;
        var ledgers = 20;
        var got_proposed;

        $.remote.transaction()
            .payment('root', 'alice', "1")
            .once('submitted', function (m) {
                // Transaction got an error.
                // console.log("proposed: %s", JSON.stringify(m));
                assert.strictEqual(m.engine_result, 'tecNO_DST_INSUF_XRP');
                got_proposed  = true;
                $.remote.ledger_accept();    // Move it along.
            })
            .once('final', function (m) {
                // console.log("final: %s", JSON.stringify(m, undefined, 2));
                assert.strictEqual(m.engine_result, 'tecNO_DST_INSUF_XRP');
                done();
            })
            .submit();
    });

    test('set InflationDest', function(done) {
        var self = this;

        var steps = [
            function (callback) {
                self.what = "Set InflationDest.";

                $.remote.transaction()
                    .account_set("root")
                    .inflation_dest($.remote.account('root')._account_id)
                    .on('submitted', function (m) {
                        //console.log("proposed: %s", JSON.stringify(m));
                        if (m.engine_result === 'tesSUCCESS') {
                            callback(null);
                        } else {
                            callback(new Error(m.engine_result));
                        }
                    })
                    .submit();
            },
            function(callback)
            {
                $.remote.ledger_accept();    // Move it along.
                callback(null);
            }
        ]

        async.waterfall(steps,function (error) {
            assert(!error, self.what);
            done();
        });
    });
});

// vim:sw=2:sts=2:ts=8:et
