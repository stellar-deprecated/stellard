var async     = require("async");
var assert    = require('assert');
var Amount    = require("stellar-lib").Amount;
var Remote    = require("stellar-lib").Remote;
var Server    = require("./server").Server;
var testutils = require("./testutils");
var config    = testutils.init_config();

suite('Deleting accounts', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  /*
  test("delete test #1", function (done) {
    var self = this;
    var starting_balance = 0;
    var tx_fee = 0;

    var steps = [
      function (callback) {
        self.what = "Create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol"], callback);
      },

      /*
       function (callback) {
       self.what = "Display ledger";

       $.remote.request_ledger('current', true)
       .on('success', function (m) {
       console.log("Ledger: %s", JSON.stringify(m, undefined, 2));
       callback();
       }).on('error', function(m) {
       console.log("error: %s", JSON.stringify(m));
       callback();
       }).request();
       }, * /

      function (callback) {
        self.what = "Get bob's state";

        $.remote.requestAccountBalance($.remote.account('bob')._account_id, 'current', null)
          .on('success', function (m) {
            starting_balance = m.node.Balance;
            callback();
          }).request();
      },

      function (callback) {
        self.what = "Delete alice";
        $.remote.transaction()
          .accountDelete('alice', 'bob')
          .once('submitted', function (m) {
            assert.strictEqual(m.engine_result, 'tesSUCCESS');
            $.remote.ledger_accept();    // Move it along.
          })
          .once('final', function (m) {
            assert.strictEqual(m.engine_result, 'tesSUCCESS');
			//console.log("TX: %s", JSON.stringify(m, undefined, 2));
            tx_fee = m.tx_json.Fee;
            callback();
          })
          .submit();
      },

      function (callback) {
        self.what = "Check bob's state";

        $.remote.requestAccountBalance($.remote.account('bob')._account_id, 'current', null)
          .on('success', function (m) {
            if(m.node.Balance != 2 * starting_balance - tx_fee)
              console.log("bob's state: "+m.node.Balance+', tx: '+tx_fee+', starting_balance:'+starting_balance );
            assert(m.node.Balance == 2 * starting_balance - tx_fee );
            callback();
          }).request();
      },

      /*
       function (callback) {
       self.what = "Display alice's state";

       $.remote.requestAccountBalance($.remote.account('alice')._account_id, 'current', null)
       .on('success', function (m) {
       console.log('success'+m);
       callback();
       })
       .on('failure', function (m) {
       console.log('failure'+m);
       callback();
       })
       .request();
       },
       * /

    ];

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });
*/
  test("delete account, create new line", function (done){
    var self = this;

    var steps = [

      function (callback) {
        self.what = "Create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "mtgox"], callback);
      },

      function (callback) {
        self.what = "Set alice's limit.";
        testutils.credit_limit($.remote, "alice", "1000/USD/mtgox", callback);
      },
/*
      function (callback) {
        self.what = "Set bob's limit.";
        testutils.credit_limit($.remote, "bob", "1000/USD/mtgox", callback);
      },*/

      function (callback) {
        self.what = "Send alice some USD.";

        $.remote.transaction()
          .payment('mtgox', 'alice', "100/USD/mtgox")
          .once('submitted', function (m) {
            //console.log("proposed:%s",JSON.stringify(m, undefined, 2));
            callback(m.engine_result !== 'tesSUCCESS');
          })
          /*.once('final', function (m) {
		    console.log("proposed:%s",JSON.stringify(m, undefined, 2));
            assert(m.engine_result !== 'tesSUCCESS');
            callback();
          })*/
          .submit();
      },
	  /*
	  function (callback) {
        self.what = "Send bob some USD.";

        $.remote.transaction()
          .payment('mtgox', 'bob', "10/USD/mtgox")
          .once('submitted', function (m) {
            //console.log("proposed:%s",JSON.stringify(m));
			//$.remote.ledger_accept();    // Move it along. //
            callback(m.engine_result !== 'tesSUCCESS');
          })
          .submit();
      },*/

      function (callback) {
        self.what = "Verify balance.";

        $.remote.request_ripple_balance("alice", "mtgox", "USD", 'CURRENT')
          .once('ripple_state', function (m) {
			//console.log("res: %s",JSON.stringify(m, undefined, 2));
            console.log(m.account_balance.to_number(false));
            assert(m.account_balance.equals("100/USD/alice"));
            assert(m.peer_balance.equals("-100/USD/mtgox"));

            callback();
          })
          .request();
      },

/*
	  function (callback) {
        self.what = "Display ledger";

        $.remote.request_ledger('current', true)
          .on('success', function (m) {
            console.log("Ledger: %s", JSON.stringify(m, undefined, 2));
            callback();
          }).on('error', function(m) {
            //console.log("error: %s", JSON.stringify(m));
            callback();
          }).request();
      },
	*/  
      function (callback) {
        self.what = "Delete alice";
        $.remote.transaction()
          .accountDelete('alice', 'bob')
          .once('submitted', function (m) {
            assert.strictEqual(m.engine_result, 'tesSUCCESS');
            //$.remote.once('ledger_closed', function(ledger_closed, ledger_index) { callback(); } );
			//$.remote.ledger_accept();    // Move it along.
			callback();
          })
          .once('final', function (m) {
			//console.log("res: %s",JSON.stringify(m, undefined, 2));
            assert.strictEqual(m.engine_result, 'tesSUCCESS');
            tx_fee = m.tx_json.Fee;
            callback();
          })
          .submit();
      },
	  
      function (callback) {
        self.what = "Verify balance.";

        $.remote.request_ripple_balance("bob", "mtgox",  "USD", 'CURRENT')
          .once('ripple_state', function (m) {
		    //console.log("res: %s",JSON.stringify(m, undefined, 2));
            console.log(m.account_balance.to_number(false));
            console.log(m.peer_balance.to_number(false));
            assert(m.account_balance.equals("100/USD/bob"));
            assert(m.peer_balance.equals("-100/USD/mtgox"));

            callback();
          })
		  .once('error' , function (m) {
			console.log("err: %s",JSON.stringify(m, undefined, 2));
			callback();
		  })
          .request();
      },
    ];

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });
  
  test("delete account, update existing line", function (done){
    var self = this;

    var steps = [

      function (callback) {
        self.what = "Create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "mtgox"], callback);
      },

      function (callback) {
        self.what = "Set alice's limit.";
        testutils.credit_limit($.remote, "alice", "1000/USD/mtgox", callback);
      },

      function (callback) {
        self.what = "Set bob's limit.";
        testutils.credit_limit($.remote, "bob", "1000/USD/mtgox", callback);
      },

      function (callback) {
        self.what = "Send alice some USD.";

        $.remote.transaction()
          .payment('mtgox', 'alice', "100/USD/mtgox")
          .once('submitted', function (m) {
            callback(m.engine_result !== 'tesSUCCESS');
          })
          .submit();
      },
	  
	  function (callback) {
        self.what = "Send bob some USD.";

        $.remote.transaction()
          .payment('mtgox', 'bob', "50/USD/mtgox")
          .once('submitted', function (m) {
            callback(m.engine_result !== 'tesSUCCESS');
          })
          .submit();
      },

      function (callback) {
        self.what = "Verify balance.";

        $.remote.request_ripple_balance("alice", "mtgox", "USD", 'CURRENT')
          .once('ripple_state', function (m) {
            //console.log(m.account_balance.to_number(false));
            assert(m.account_balance.equals("100/USD/alice"));
            assert(m.peer_balance.equals("-100/USD/mtgox"));

            callback();
          })
          .request();
      },


      function (callback) {
        self.what = "Delete alice";
        $.remote.transaction()
          .accountDelete('alice', 'bob')
          .once('submitted', function (m) {
            assert.strictEqual(m.engine_result, 'tesSUCCESS');
			callback();
          })
          .once('final', function (m) {
            assert.strictEqual(m.engine_result, 'tesSUCCESS');
            callback();
          })
          .submit();
      },
	  
      function (callback) {
        self.what = "Verify balance.";

        $.remote.request_ripple_balance("bob", "mtgox",  "USD", 'CURRENT')
          .once('ripple_state', function (m) {
            assert(m.account_balance.equals("150/USD/bob"));
            assert(m.peer_balance.equals("-150/USD/mtgox"));

            callback();
          })
		  .once('error' , function (m) {
			console.log("err: %s",JSON.stringify(m, undefined, 2));
			callback(m);
		  })
          .request();
      },
    ];

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });

});


// vim:sw=2:sts=2:ts=8:et
