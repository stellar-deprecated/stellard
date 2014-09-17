var async     = require("async");
var assert    = require('assert');
var Amount    = require("stellar-lib").Amount;
var Remote    = require("stellar-lib").Remote;
var Server    = require("./server").Server;
var testutils = require("./testutils");
var config    = testutils.init_config();

suite('Merging accounts', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });


  function accountCreatorHelper( t ) {
  	var r = [
		function (callback) {
			t.what = "Create accounts.";
			testutils.create_accounts( $.remote, "root", "10000.0", ["alice", "bob", t.gateways[0], t.gateways[1]], callback );
		},
  		function (callback) {
  			t.what = "Set limits";
  			var limits = {
  				"alice": ["100/USD/" + t.gateways[0], "111/USD/" + t.gateways[1]]
  			};
  			limits[t.gateways[0]] = "49/USD/alice";
  			limits[t.gateways[1]] = "50/USD/alice";
  			testutils.credit_limits( $.remote, limits, callback );
  		},
		function ( callback ) {
			t.what = "Distribute funds.";
			var dist = {};
			dist[t.gateways[0]] = ["99/USD/alice"];
			testutils.payments( $.remote, dist, callback );
		},
		
		function ( callback ) {
		 	t.what = "Create offer.";

		 	$.remote.transaction()
              .offer_create( "alice", "10/USD/mtgox", "500" )
              .on( 'submitted', function ( m ) {
              	assert.strictEqual( m.engine_result, 'tesSUCCESS' );
              	callback();
              })
              .submit();
		 }
  	];
  	return { steps: r } ;
  }

  function test_helper( t, gateways, merge_should_fail, extra_steps, post_checks) {
  	t.starting_balance = 0;
  	t.starting_dest_balance = 0;
  	t.tx_fee = 0;

  	t.alice_id = $.remote.account( 'alice' )._account_id;

  	t.gateways = gateways;

    var ac = accountCreatorHelper( t );

    var steps = ac.steps.concat( [

		function ( callback ) {
			t.what = "Set Bob's credit limits";
			testutils.credit_limits( $.remote,
				{
					"bob": ["200/USD/" + gateways[0], "201/USD/" + gateways[1]]
				},
				callback );
		},
		function ( callback ) {
			t.what = "Distribute funds.";

			var dist = {};
			dist[gateways[0]] = ["100/USD/bob"];
			testutils.payments( $.remote, dist, callback );
		}],
		extra_steps,
		[
		function ( callback ) {
			t.what = "Get alice's state";

			$.remote.requestAccountBalance( $.remote.account( 'alice' )._account_id, 'current', null )
			  .on( 'success', function ( m ) {
			  	t.starting_balance = m.node.Balance;
			  	callback();
			  } ).request();
		},
		function ( callback ) {
			t.what = "Get bob's state";

			$.remote.requestAccountBalance( $.remote.account( 'bob' )._account_id, 'current', null )
			  .on( 'success', function ( m ) {
			  	t.starting_dest_balance = m.node.Balance;
			  	callback();
			  } ).request();
		},
		function ( callback ) {
			$.remote.once( 'ledger_closed', function ( ledger_closed, ledger_index ) { callback(); } );
			$.remote.ledger_accept();    // Move it along.
		},

//		testutils.display_ledger_helper( t, $.remote, "original ledger" ),
		
      function ( callback ) {
      	t.what = "Merge alice -> bob";
      	$.remote.transaction()
          .accountMerge( 'alice', 'bob' )
          .once( 'submitted', function ( m ) {
          	if (merge_should_fail) {
          		assert( m.engine_result != 'tesSUCCESS' );
          	} else {
          		assert.strictEqual( m.engine_result, 'tesSUCCESS' );
          	}
          	t.tx_fee = m.tx_json.Fee;
          	$.remote.once( 'ledger_closed', function ( ledger_closed, ledger_index ) { callback(); } );
          	$.remote.ledger_accept();    // Move it along.
          } )
          .submit();
      },
	  
//	  testutils.display_ledger_helper( t, $.remote, "final ledger"),

      
		], post_checks );

  	return steps;
  }

  function checks_mergeSuccess( t ) {
  	return [function ( callback ) {
  		t.what = "Check bob's STR balance";

  		$.remote.requestAccountBalance( $.remote.account( 'bob' )._account_id, 'current', null )
		  .on( 'success', function ( m ) {
		  	var predicted_balance = t.starting_balance - t.tx_fee + Number( t.starting_dest_balance ); // fee here is for account merge operation
		  	//console.log( "bob's balance: " + m.node.Balance + ', predicted:' + predicted_balance + ', tx: ' + t.tx_fee + ', original dest balance:' + t.starting_dest_balance + ', original account balance:' + t.starting_balance );

		  	assert( m.node.Balance == predicted_balance );
		  	callback();
		  } ).request();
  	},

	 function ( callback ) {
	 	t.what = "Verify balances";

	 	var expected = {};
	 	expected["bob"] = ["199/USD/" + t.gateways[0], "0/USD/" + t.gateways[1]];
	 	expected[t.gateways[0]] = "-199/USD/bob";
	 	testutils.verify_balances( $.remote, expected, callback );
	 },

	 function ( callback ) {
	 	t.what = "check that alice is deleted";

	 	// the way we test it here is by looking for the id as we want any reference to the account to be gone
	 	helper = testutils.custom_ledger_helper( t, $.remote, null, function ( ledger, cb ) {
	 		s = JSON.stringify( ledger, undefined );
	 		assert( s.indexOf( t.alice_id ) == -1 );
	 		cb();
	 	} );
	 	helper( callback );
	 }];
  }

  function checks_mergefail( t, get_expected ) {
  	return [function ( callback ) {
  		t.what = "Check bob's STR balance";

  		$.remote.requestAccountBalance( $.remote.account( 'bob' )._account_id, 'current', null )
		  .on( 'success', function ( m ) {
		  	assert( m.node.Balance == t.starting_dest_balance );
		  	callback();
		  } ).request();
  	},

	 function ( callback ) {
	 	t.what = "Verify balances";

	 	var expected = get_expected();
	 	
	 	testutils.verify_balances( $.remote, expected, callback );
	 },

  	];
  }

	// tests are deleting "alice", transfering to "bob"
  test("simple merge", function (done) {
  	var self = this;
  	var steps = test_helper(self, ["mtgox", "bitstamp"], false, [], checks_mergeSuccess(self));

    async.waterfall( steps, function ( error ) {
    	if ( error )
    		console.log( error );
      assert(!error, self.what);
      done();
    });
  });

	
  test( "simple merge #2", function ( done ) {
  	var self = this;

  	var steps = test_helper( self, ["bitstamp", "mtgox"], false, [], checks_mergeSuccess(self) );


  	async.waterfall( steps, function ( error ) {
  		if ( error )
  			console.log( error );
  		assert( !error, self.what );
  		done();
  	} );
  } );
  

function iouTestHelper_negative_balance( t, gateways ) {
	var get_expected = function () {
		var expected = {
			"bob": ["100/USD/" + t.gateways[0], "0/USD/" + t.gateways[1], t.starting_dest_balance],
			"alice": ["-26/USD/" + t.gateways[0], "0/USD/" + t.gateways[1], t.starting_balance]
		};
		expected[t.gateways[0]] = ["-100/USD/bob", "26/USD/alice"];
		//expected[t.gateways[1]] = [];
		return expected;
	}
	return steps = test_helper( t, gateways, true, [
		function ( callback ) {
			t.what = "Distribute funds 2";
			var dist = { "alice": ["125/USD/" + gateways[0]] };
			testutils.payments( $.remote, dist, callback );
		},
	], checks_mergefail( t, get_expected ) );
}

test( "merge user with negative IOU #1", function ( done ) {
	var self = this;
	var steps = iouTestHelper_negative_balance( this, ["bitstamp", "mtgox"] );
  	async.waterfall( steps, function ( error ) {
  		if ( error ) {
  			console.log( JSON.stringify( error, undefined ) );
  		}
  		assert( !error, self.what );
  		done();
  	} );
  } );

  test( "merge user with negative IOU #2", function ( done ) {
  	var self = this;
  	var steps = iouTestHelper_negative_balance( this, ["mtgox", "bitstamp"] );
  	async.waterfall( steps, function ( error ) {
  		if ( error ) {
  			console.log( JSON.stringify( error, undefined ) );
  		}
  		assert( !error, self.what );
  		done();
  	} );
  } );
  
function iouTestHelper_overlimit( t, gateways ) {
	var get_expected = function () {
		var expected = {
			"bob": ["100/USD/" + t.gateways[0], "0/USD/" + t.gateways[1], t.starting_dest_balance],
			"alice": ["99/USD/" + t.gateways[0], "0/USD/" + t.gateways[1], t.starting_balance]
		};
		expected[t.gateways[0]] = ["-100/USD/bob", "-99/USD/alice"];
		//expected[t.gateways[1]] = [];
		return expected;
	}

  	return steps = test_helper( t, gateways, true, [
		function ( callback ) {
			t.what = "Set Bob's credit limits";
			testutils.credit_limits( $.remote,
				{
					"bob": ["110/USD/" + gateways[0], "201/USD/" + gateways[1]]
				},
				callback );
		}
  	], checks_mergefail( t, get_expected ) );
  }

  test( "merge user with over the limit IOU #1", function ( done ) {
  	var self = this;
  	var steps = iouTestHelper_overlimit( this, ["bitstamp", "mtgox"] );
  	async.waterfall( steps, function ( error ) {
  		if ( error ) {
  			console.log( JSON.stringify( error, undefined ) );
  		}
  		assert( !error, self.what );
  		done();
  	} );
  } );

  test( "merge user with over the limit IOU #2", function ( done ) {
  	var self = this;
  	var steps = iouTestHelper_overlimit( this, ["mtgox", "bitstamp"] );
  	async.waterfall( steps, function ( error ) {
  		if ( error ) {
  			console.log( JSON.stringify( error, undefined ) );
  		}
  		assert( !error, self.what );
  		done();
  	} );
  } );
});



// vim:sw=2:sts=2:ts=8:et
