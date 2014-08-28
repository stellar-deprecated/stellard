var async     = require("async");
var assert    = require('assert');
var Amount    = require("stellar-lib").Amount;
var Remote    = require("stellar-lib").Remote;
var Server    = require("./server").Server;
var unirest   = require('unirest');
var Promise   = require('bluebird');
var testutils = require("./testutils");
var config    = testutils.init_config();



suite('Sending', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("send STR to non-existent account with insufficient fee", function (done) {
    var self    = this;
    var ledgers = 20;
    var got_proposed;

    $.remote.transaction()
    .payment('root', 'alice', "1")
    .once('submitted', function (m) {
      // Transaction got an error.
      // console.log("proposed: %s", JSON.stringify(m));
      assert.strictEqual(m.engine_result, 'tecNO_DST_INSUF_STR');
      got_proposed  = true;
      $.remote.ledger_accept();    // Move it along.
    })
    .once('final', function (m) {
      // console.log("final: %s", JSON.stringify(m, undefined, 2));
      assert.strictEqual(m.engine_result, 'tecNO_DST_INSUF_STR');
      done();
    })
    .submit();
  });

  // Also test transaction becomes lost after tecNO_DST.
  test("credit_limit to non-existent account = tecNO_DST", function (done) {
    $.remote.transaction()
    .ripple_line_set("root", "100/USD/alice")
    .once('submitted', function (m) {
      //console.log("proposed: %s", JSON.stringify(m));
      assert.strictEqual(m.engine_result, 'tecNO_DST');
      done();
    })
    .submit();
  });

  test("credit_limit", function (done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = "Create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
      },

      function (callback) {
        self.what = "Check a non-existent credit limit.";

        $.remote.request_ripple_balance("alice", "mtgox", "USD", 'CURRENT')
        .on('ripple_state', function (m) {
          callback(new Error(m));
        })
        .on('error', function(m) {
          // console.log("error: %s", JSON.stringify(m));

          assert.strictEqual('remoteError', m.error);
          assert.strictEqual('entryNotFound', m.remote.error);
          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "Create a credit limit.";
        testutils.credit_limit($.remote, "alice", "800/USD/mtgox", callback);
      },

      function (callback) {
        $.remote.request_ripple_balance("alice", "mtgox", "USD", 'CURRENT')
        .on('ripple_state', function (m) {
          //                console.log("BALANCE: %s", JSON.stringify(m));
          //                console.log("account_balance: %s", m.account_balance.to_text_full());
          //                console.log("account_limit: %s", m.account_limit.to_text_full());
          //                console.log("peer_balance: %s", m.peer_balance.to_text_full());
          //                console.log("peer_limit: %s", m.peer_limit.to_text_full());
          assert(m.account_balance.equals("0/USD/alice"));
          assert(m.account_limit.equals("800/USD/mtgox"));
          assert(m.peer_balance.equals("0/USD/mtgox"));
          assert(m.peer_limit.equals("0/USD/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "Modify a credit limit.";
        testutils.credit_limit($.remote, "alice", "700/USD/mtgox", callback);
      },

      function (callback) {
        $.remote.request_ripple_balance("alice", "mtgox", "USD", 'CURRENT')
        .on('ripple_state', function (m) {
          assert(m.account_balance.equals("0/USD/alice"));
          assert(m.account_limit.equals("700/USD/mtgox"));
          assert(m.peer_balance.equals("0/USD/mtgox"));
          assert(m.peer_limit.equals("0/USD/alice"));

          callback();
        })
        .request();
      },
      // Set negative limit.
      function (callback) {
        $.remote.transaction()
        .ripple_line_set("alice", "-1/USD/mtgox")
        .once('submitted', function (m) {
          assert.strictEqual('temBAD_LIMIT', m.engine_result);
          callback();
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
        self.what = "Zero a credit limit.";
        testutils.credit_limit($.remote, "alice", "0/USD/mtgox", callback);
      },

      function (callback) {
        self.what = "Make sure line is deleted.";

        $.remote.request_ripple_balance("alice", "mtgox", "USD", 'CURRENT')
        .on('ripple_state', function (m) {
          // Used to keep lines.
          // assert(m.account_balance.equals("0/USD/alice"));
          // assert(m.account_limit.equals("0/USD/alice"));
          // assert(m.peer_balance.equals("0/USD/mtgox"));
          // assert(m.peer_limit.equals("0/USD/mtgox"));
          callback(new Error(m));
        })
        .on('error', function (m) {
          // console.log("error: %s", JSON.stringify(m));
          assert.strictEqual('remoteError', m.error);
          assert.strictEqual('entryNotFound', m.remote.error);

          callback();
        })
        .request();
      },
      // TODO Check in both owner books.
      function (callback) {
        self.what = "Set another limit.";
        testutils.credit_limit($.remote, "alice", "600/USD/bob", callback);
      },

      function (callback) {
        self.what = "Set limit on other side.";
        testutils.credit_limit($.remote, "bob", "500/USD/alice", callback);
      },

      function (callback) {
        self.what = "Check ripple_line's state from alice's pov.";

        $.remote.request_ripple_balance("alice", "bob", "USD", 'CURRENT')
        .on('ripple_state', function (m) {
          // console.log("proposed: %s", JSON.stringify(m));

          assert(m.account_balance.equals("0/USD/alice"));
          assert(m.account_limit.equals("600/USD/bob"));
          assert(m.peer_balance.equals("0/USD/bob"));
          assert(m.peer_limit.equals("500/USD/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "Check ripple_line's state from bob's pov.";

        $.remote.request_ripple_balance("bob", "alice", "USD", 'CURRENT')
        .on('ripple_state', function (m) {
          assert(m.account_balance.equals("0/USD/bob"));
          assert(m.account_limit.equals("500/USD/alice"));
          assert(m.peer_balance.equals("0/USD/alice"));
          assert(m.peer_limit.equals("600/USD/bob"));

          callback();
        })
        .request();
      }
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });
});

suite('Sending future', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test('direct ripple', function(done) {
    var self = this;

    // $.remote.set_trace();

    var steps = [
      function (callback) {
        self.what = "Create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
      },

      function (callback) {
        self.what = "Set alice's limit.";
        testutils.credit_limit($.remote, "alice", "600/USD/bob", callback);
      },

      function (callback) {
        self.what = "Set bob's limit.";
        testutils.credit_limit($.remote, "bob", "700/USD/alice", callback);
      },

      function (callback) {
        self.what = "Set alice send bob partial with alice as issuer.";

        $.remote.transaction()
        .payment('alice', 'bob', "24/USD/alice")
        .once('submitted', function (m) {
          // console.log("proposed: %s", JSON.stringify(m));
          callback(m.engine_result !== 'tesSUCCESS');
        })
        .once('final', function (m) {
          assert(m.engine_result !== 'tesSUCCESS');
        })
        .submit();
      },

      function (callback) {
        self.what = "Verify balance.";

        $.remote.request_ripple_balance("alice", "bob", "USD", 'CURRENT')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("-24/USD/alice"));
          assert(m.peer_balance.equals("24/USD/bob"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "Set alice send bob more with bob as issuer.";

        $.remote.transaction()
        .payment('alice', 'bob', "33/USD/bob")
        .once('submitted', function (m) {
          // console.log("proposed: %s", JSON.stringify(m));
          callback(m.engine_result !== 'tesSUCCESS');
        })
        .once('final', function (m) {
          assert(m.engine_result !== 'tesSUCCESS');
        })
        .submit();
      },

      function (callback) {
        self.what = "Verify balance from bob's pov.";

        $.remote.request_ripple_balance("bob", "alice", "USD", 'CURRENT')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("57/USD/bob"));
          assert(m.peer_balance.equals("-57/USD/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "Bob send back more than sent.";

        $.remote.transaction()
        .payment('bob', 'alice', "90/USD/bob")
        .once('submitted', function (m) {
          // console.log("proposed: %s", JSON.stringify(m));
          callback(m.engine_result !== 'tesSUCCESS');
        })
        .once('final', function (m) {
          assert(m.engine_result !== 'tesSUCCESS');
        })
        .submit();
      },

      function (callback) {
        self.what = "Verify balance from alice's pov: 1";

        $.remote.request_ripple_balance("alice", "bob", "USD", 'CURRENT')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("33/USD/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "Alice send to limit.";

        $.remote.transaction()
        .payment('alice', 'bob', "733/USD/bob")
        .once('submitted', function (m) {
          // console.log("submitted: %s", JSON.stringify(m));
          callback(m.engine_result !== 'tesSUCCESS');
        })
        .once('final', function (m) {
          assert(m.engine_result !== 'tesSUCCESS');
        })
        .submit();
      },

      function (callback) {
        self.what = "Verify balance from alice's pov: 2";

        $.remote.request_ripple_balance("alice", "bob", "USD", 'CURRENT')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("-700/USD/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "Bob send to limit.";

        $.remote.transaction()
        .payment('bob', 'alice', "1300/USD/bob")
        .once('submitted', function (m) {
          // console.log("submitted: %s", JSON.stringify(m));
          callback(m.engine_result !== 'tesSUCCESS');
        })
        .once('final', function (m) {
          assert(m.engine_result !== 'tesSUCCESS');
        })
        .submit();
      },

      function (callback) {
        self.what = "Verify balance from alice's pov: 3";

        $.remote.request_ripple_balance("alice", "bob", "USD", 'CURRENT')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("600/USD/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        // If this gets applied out of order, it could stop the big payment.
        self.what = "Bob send past limit.";

        $.remote.transaction()
        .payment('bob', 'alice', "1/USD/bob")
        .once('submitted', function (m) {
          // console.log("submitted: %s", JSON.stringify(m));
          callback(m.engine_result !== 'tecPATH_DRY');
        })
        .submit();
      },

      function (callback) {
        self.what = "Verify balance from alice's pov: 4";

        $.remote.request_ripple_balance("alice", "bob", "USD", 'CURRENT')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("600/USD/alice"));

          callback();
        })
        .request();
      },

      //        function (callback) {
      //          // Make sure all is good after canonical ordering.
      //          self.what = "Close the ledger and check balance.";
      //
      //          $.remote
      //            .once('ledger_closed', function (message) {
      //                // console.log("LEDGER_CLOSED: A: %d: %s", ledger_closed_index, ledger_closed);
      //                callback();
      //              })
      //            .ledger_accept();
      //        },
      //        function (callback) {
      //          self.what = "Verify balance from alice's pov: 5";
      //
      //          $.remote.request_ripple_balance("alice", "bob", "USD", 'CURRENT')
      //            .once('ripple_state', function (m) {
      //                console.log("account_balance: %s", m.account_balance.to_text_full());
      //                console.log("account_limit: %s", m.account_limit.to_text_full());
      //                console.log("peer_balance: %s", m.peer_balance.to_text_full());
      //                console.log("peer_limit: %s", m.peer_limit.to_text_full());
      //
      //                assert(m.account_balance.equals("600/USD/alice"));
      //
      //                callback();
      //              })
      //            .request();
      //        },
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });
});





// vim:sw=2:sts=2:ts=8:et
