var async     = require("async");
var assert    = require('assert');
var Amount    = require("stellar-lib").Amount;
var Remote    = require("stellar-lib").Remote;
var Server    = require("./server").Server;
var testutils = require("./testutils");
var config    = testutils.init_config();

suite('Inflation', function() {
    var $ = { };
    var TOTAL_MONEY = 100000000000000000;
    var INFLATION_RATE = 0.000817609695;
    var INFLATION_ROUND = TOTAL_MONEY * INFLATION_RATE;
/*  var INFLATION_UP_LIMIT = INFLATION_ROUND * 1.0001;
    var INFLATION_DOWN_LIMIT = INFLATION_ROUND * 0.9999;
*/
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


  test('Inflation #1 - all to one destination', function(done) {
    var self = this;
    var start_balance = 0;
    var tx_fee = 15; //TODO: get tx fee

    var steps = [

      function (callback) {
        self.what = "Create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "mtgox"], callback);
      },

      function (callback) {
        self.what = "Set InflationDest #1";

        $.remote.transaction()
          .account_set("alice")
          .inflation_dest($.remote.account('carol')._account_id)
          .on('submitted', function (m) {
            if (m.engine_result === 'tesSUCCESS') {
              $.remote.ledger_accept();    // Move it along.
              callback(null);
            } else {
              callback(new Error(m.engine_result));
            }
          })
          .submit();
      },

      function (callback) {
        self.what = "Set InflationDest #2";

        $.remote.transaction()
          .account_set("bob")
          .inflation_dest($.remote.account('carol')._account_id)
          .on('submitted', function (m) {
            if (m.engine_result === 'tesSUCCESS') {
              $.remote.ledger_accept();    // Move it along.
              callback(null);
            } else {
              callback(new Error(m.engine_result));
            }
          })
          .submit();
      },

      function (callback) {
        self.what = "Check starting balance";

        $.remote.requestAccountBalance($.remote.account('carol')._account_id, 'current', null)
          .on('success', function (m) {
            start_balance = m.node.Balance;
            callback();
          }).request();
      },

      function (callback) {
        self.what = "Do inflation";

        $.remote.transaction()
          .inflation($.remote.account('root')._account_id, 1)
          .on('submitted', function (m) {
            if (m.engine_result === 'tesSUCCESS') {
              callback(null);
            } else {
              callback(new Error(m.engine_result));
            }
          })
          .on('error', function (m) {
            console.log('error: %s', JSON.stringify(m));
            callback(m);
          })
          .submit();
      },

      function (callback) {
        self.what = "Check final balance";

        $.remote.requestAccountBalance($.remote.account('carol')._account_id, 'current', null)
          .on('success', function (m) {

            var diff = m.node.Balance - start_balance;

            assert( diff >= INFLATION_ROUND );
            assert( diff < (INFLATION_ROUND + tx_fee*15) ); // 10 tx fees

            callback();
          }).request();
      },

      function(callback)
      {
        $.remote.ledger_accept();    // Move it along.
        callback(null);
      },
    ]

    async.waterfall(steps,function (error) {
      assert(!error, self.what);
      done();
    });
  });

  test('Inflation #2 - 50/50 split', function(done) {
    var self = this;
    var start_balance = 10000000000;
    var tx_fee = 15; //TODO: get tx fee

    var steps = [

      function (callback) {
        self.what = "Create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "mtgox"], callback);
      },

      function (callback) {
        self.what = "Set InflationDest #1";

        $.remote.transaction()
          .account_set("alice")
          .inflation_dest($.remote.account('carol')._account_id)
          .on('submitted', function (m) {
            if (m.engine_result === 'tesSUCCESS') {
              $.remote.ledger_accept();    // Move it along.
              callback(null);
            } else {
              callback(new Error(m.engine_result));
            }
          })
          .submit();
      },

      function (callback) {
        self.what = "Set InflationDest #2";

        $.remote.transaction()
          .account_set("bob")
          .inflation_dest($.remote.account('mtgox')._account_id)
          .on('submitted', function (m) {
            if (m.engine_result === 'tesSUCCESS') {
              $.remote.ledger_accept();    // Move it along.
              callback(null);
            } else {
              callback(new Error(m.engine_result));
            }
          })
          .submit();
      },

      function (callback) {
        self.what = "Do inflation";

        $.remote.transaction()
          .inflation($.remote.account('root')._account_id, 1)
          .on('submitted', function (m) {
            if (m.engine_result === 'tesSUCCESS') {
              $.remote.ledger_accept();    // Move it along.
              callback(null);
            } else {
              callback(new Error(m.engine_result));
            }
          })
          .on('error', function (m) {
            console.log('error: %s', JSON.stringify(m));
            callback(m);
          })
          .submit();
      },

      function (callback) {
        self.what = "Display ledger";

        $.remote.request_ledger('current', true)
          .on('success', function (m) {
            //console.log("Ledger: %s", JSON.stringify(m, undefined, 2));
            callback();
          }).on('error', function(m) {
            console.log("error: %s", JSON.stringify(m));
            callback();
          }).request();
      },

      function (callback) {
        self.what = "Check mtgox's balance"; // async task, could take forever :)

        $.remote.requestAccountBalance($.remote.account('mtgox')._account_id, 'current', null)
          .on('success', function (m) {

            var diff = m.node.Balance - start_balance; // diff == -850,000BTC
            //console.log(diff);

            assert( diff >= INFLATION_ROUND/2 );
            assert( diff < (INFLATION_ROUND/2 + tx_fee*15) ); // 10 tx fees

            callback();
          }).request();
      },


      function (callback) {
        self.what = "Check carol's balance";

        $.remote.requestAccountBalance($.remote.account('carol')._account_id, 'current', null)
          .on('success', function (m) {

            var diff = m.node.Balance - start_balance;
            //console.log(diff);

            assert( diff >= INFLATION_ROUND/2 );
            assert( diff < (INFLATION_ROUND/2 + tx_fee*15) ); // 10 tx fees

            callback();
          }).request();
      },

    ]

    async.waterfall(steps,function (error) {
      assert(!error, self.what);
      done();
    });
  });

});

// vim:sw=2:sts=2:ts=8:et
