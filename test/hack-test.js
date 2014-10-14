var async     = require("async");
var assert    = require('assert');
var Amount    = require("stellar-lib").Amount;
var Remote    = require("stellar-lib").Remote;
var Server    = require("./server").Server;
var unirest   = require('unirest');
var Promise   = require('bluebird');
var testutils = require("./testutils");
var config    = testutils.init_config();



suite('Hack', function() {
    var $ = { };

    setup(function(done) {
        testutils.build_setup().call($, done);
    });

    teardown(function(done) {
        testutils.build_teardown().call($, done);
    });
/*
    test("try to create IOUs out of thin air", function (done) {
        var self = this;

        var steps = [
            function (callback) {
                self.what = "Create accounts.";
                testutils.create_accounts($.remote, "root", "10000.0", ["alice", "mtgox"], callback);
            },

            function (callback) {
                self.what = "Alice trusts gateway";
                testutils.credit_limit($.remote, "alice", "1000/USD/mtgox", callback);
            },

            function (callback) {
                self.what = "Gateway funds alice";
                var tx =
                 '{ "method": "submit",                                           \
                    "params": [                                                         \
                    { "secret": "sfHTRZFpsX7yc2VJLvhJzpzjiag5W6ofy9xgZSwuubcLhS1atmv",  \
                        "tx_json": {                                                    \
                            "TransactionType": "Payment",                               \
                            "Account" : "gD1RB8jG5DTSEnjJ1PQyoHEWXGbGCfCLAZ",           \
                            "Amount" : {\
                                "currency" : "USD",\
                                "issuer" : "gD1RB8jG5DTSEnjJ1PQyoHEWXGbGCfCLAZ", \
                                "value" : "1"\
                                },\
                            "Destination" : "gpRqvep69x2dpuVw7jEsVbSUgpfisTPwZa", \
                            "Fee" : "10"\
                             }}]}';

                console.log("hi");
                testutils.rpc(config,tx)
                    .then(function(result){
                        //console.log('ledger1 '+ JSON.stringify(result));
                        return testutils.rpc(config,'{"method":"ledger_accept"}'); })
                    .then(function(result){ callback() });
            },
            function (callback) {
                self.what = "Alice funds gateway fail";

                var tx = '{ "method": "submit",                                           \
                    "params": [                                                         \
                    { "secret": "sfXvgQoXYnjqD3TeazmV3bUdK6qgfsRTa9BAxqZtbepfk7yyYZS",  \
                        "tx_json": {                                                    \
                            "TransactionType": "Payment",                               \
                            "Account" : "gpRqvep69x2dpuVw7jEsVbSUgpfisTPwZa",           \
                            "Amount" : {\
                                "currency" : "USD",\
                                "issuer" : "gD1RB8jG5DTSEnjJ1PQyoHEWXGbGCfCLAZ", \
                                "value" : "100000"\
                                },\
                            "Destination" : "gD1RB8jG5DTSEnjJ1PQyoHEWXGbGCfCLAZ", \
                            "Fee" : "10",\
                            "Flags" : 0 \
                             }}]}';

                testutils.rpc(config,tx)
                    .then(function(result){
                        console.log('result '+ JSON.stringify(result));
                        return testutils.rpc(config,'{"method":"ledger_accept"}'); })
                    .then(function(result){ callback() });
            },

            function (callback) {
                self.what = "Alice funds gateway";

                var tx = '{ "method": "submit",                                           \
                    "params": [                                                         \
                    { "secret": "sfXvgQoXYnjqD3TeazmV3bUdK6qgfsRTa9BAxqZtbepfk7yyYZS",  \
                        "tx_json": {                                                    \
                            "TransactionType": "Payment",                               \
                            "Account" : "gpRqvep69x2dpuVw7jEsVbSUgpfisTPwZa",           \
                            "Amount" : {\
                                "currency" : "USD",\
                                "issuer" : "gD1RB8jG5DTSEnjJ1PQyoHEWXGbGCfCLAZ", \
                                "value" : "100000"\
                                },\
                            "Destination" : "gD1RB8jG5DTSEnjJ1PQyoHEWXGbGCfCLAZ", \
                            "Fee" : "10",\
                            "Flags" : 131072 \
                             }}]}';

                testutils.rpc(config,tx)
                    .then(function(result){
                        console.log('result '+ JSON.stringify(result));
                        return testutils.rpc(config,'{"method":"ledger_accept"}'); })
                    .then(function(result){ callback() });
            },

            function (callback) {
                self.what = "Check outcome.";
                var tx= '{"method":"account_lines", "params":[ {"account":"gD1RB8jG5DTSEnjJ1PQyoHEWXGbGCfCLAZ" }]  }';
                testutils.rpc(config,tx)
                    .then(function(result) {
                        console.log('result ' + JSON.stringify(result));
                    });
            },
        ]

        async.waterfall(steps, function(error) {
            assert(!error, self.what);
            done();
        });
    });
    */
});





// vim:sw=2:sts=2:ts=8:et
