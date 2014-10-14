var async       = require('async');
var assert      = require('assert');
var extend      = require('extend');
var Amount      = require('stellar-lib').Amount;
var Remote      = require('stellar-lib').Remote;
var Transaction = require('stellar-lib').Transaction;
var Server      = require('./server').Server;
var Promise     = require('bluebird');
var unirest   = require('unirest');

var server      = { };

function get_config() {
  var cfg = require(__dirname + '/config-example');


  // See if the person testing wants to override the configuration by creating a
  // file called test/config.js.
  try {
    cfg = extend({}, cfg, require(__dirname + '/config'));
  } catch (e) { }

    //console.log('',cfg);
  return cfg;
};

function init_config() {
  return require('stellar-lib').config.load(get_config());
};

function prepare_tests(tests, fn) {
  var tests = typeof tests === 'string' ? [ tests ] : tests;
  var result = [ ];
  for (var i in tests) {
    result.push(fn(tests[i], i));
  }
  return result;
};

function account_dump(remote, account, callback) {
  var self = this;

  this.what = 'Get latest account_root';

  var request = remote.request_ledger_entry('account_root');
  request.ledger_hash(remote.ledger_hash());
  request.account_root('root');
  request.callback(function(err, r) {
    assert(!err, self.what);
    if (err) {
      //console.log('error: %s', m);
      callback(err);
    } else {
      //console.log('account_root: %s', JSON.stringify(r, undefined, 2));
      callback();
    }
  });

  // get closed ledger hash
  // get account root
  // construct a json result
};

/**
 * Helper called by test cases to generate a setUp routine.
 *
 * By default you would call this without options, but it is useful to
 * be able to plug options in during development for quick and easy
 * debugging.
 *
 * @example
 *   buster.testCase('Foobar', {
 *     setUp: testutils.build_setup({verbose: true}),
 *     // ...
 *   });
 *
 * @param opts {Object} These options allow quick-and-dirty test-specific
 *   customizations of your test environment.
 * @param opts.verbose {Bool} Enable all debug output (then cover your ears
 *   and run)
 * @param opts.verbose_ws {Bool} Enable tracing in the Remote class. Prints
 *   websocket traffic.
 * @param opts.verbose_server {Bool} Set the -v option when running rippled.
 * @param opts.no_server {Bool} Don't auto-run rippled.
 * @param host {String} Identifier for the host configuration to be used.
 */
function build_setup(opts, host) {
  var config = get_config();
  var host = host || config.server_default;
  var opts = opts || {};

  // Normalize options
  if (opts.verbose) {
    opts.verbose_ws     = true;
    opts.verbose_server = true;
  };

  function setup(done) {
    var self = this;

    self.compute_fees_amount_for_txs = function(txs) {
      var fee_units = Transaction.fee_units['default'] * txs;
      return self.remote.fee_tx(fee_units);
    };

    self.amount_for = function(options) {
      var reserve = self.remote.reserve(options.ledger_entries || 0);
      var fees = self.compute_fees_amount_for_txs(options.default_transactions || 0)
      return reserve.add(fees).add(options.extra || 0);
    };

    this.store = this.store || {};
    this.store[host] = this.store[host] || { };
    var data = this.store[host];

    data.opts = opts;

    var series = [
      function run_server(callback) {
        if (opts.no_server)  {
          return callback();
        }

        var server_config = extend({}, config.default_server_config, config.servers[host]);

        data.server = Server.from_config(host, server_config, !!opts.verbose_server);

        data.server.once('started', function() {
          callback();
        });

        data.server.once('exited', function () {
          // If know the remote, tell it server is gone.
          if (self.remote) {
            self.remote.setServerFatal();
          }
        });

        server[host] = data.server;
        data.server.start(host.dontSpawn);
      },

      function connect_websocket(callback) {
        self.remote = data.remote = Remote.from_config(host, !!opts.verbose_ws);
        self.remote.once('ledger_closed', function(ledger) {
          callback();
        });
        self.remote.connect();
      }
    ];

    async.series(series, done);
  };

  return setup;
};

/**
 * Generate tearDown routine.
 *
 * @param host {String} Identifier for the host configuration to be used.
 */
function build_teardown(host) {
  var config = get_config();
  var host = host || config.server_default;

  function teardown(done) {
    var data = this.store[host];
    var opts = data.opts;

    var series = [
      function disconnect_websocket(callback) {
        data.remote.once('disconnected', callback)
        data.remote.once('error', function (m) {
          //console.log('server error: ', m);
        })
        data.remote.connect(false);
      },

      function stop_server(callback) {
        if (opts.no_server) {
          callback();
        } else {
          data.server.once('stopped', callback)
          data.server.stop();
          delete server[host];
        }
      }
    ];

    async.series(series, done);
  };

  return teardown;
};

function create_accounts(remote, src, amount, accounts, callback) {
  assert(arguments.length === 5);

  remote.set_account_seq(src, 1);

  async.forEach(accounts, function (account, callback) {
    // Cache the seq as 1.
    // Otherwise, when other operations attempt to operate async against the account they may get confused.
    remote.set_account_seq(account, 1);

    var tx = remote.transaction();

    tx.payment(src, account, amount);

    tx.once('proposed', function (m) {
      //console.log('proposed: %s', JSON.stringify(m));
      callback(m.engine_result === 'tesSUCCESS' ? null : new Error());
    });

    tx.once('error', function (m) {
      //console.log('error: %s', JSON.stringify(m));
      callback(m);
    });

    tx.submit();
  }, callback);
};

// Account should be {name,balance}
function createAccountsFromObjects(remote, src, accounts, callback) {
    assert(arguments.length === 4);

    remote.set_account_seq(src, 1);

    var config = init_config();
    async.eachSeries(accounts, function (account, callback) {
        // Cache the seq as 1.
    	// Otherwise, when other operations attempt to operate async against the account they may get confused.
    	//console.log( "Create account %s", JSON.stringify( account ) );

		// interestingly enough, accounts must be in the config thanks to resolution embedded in uint160
    	if ( ! account.name in config.accounts )
    	{
    		console.log( "name %s not found in config ", account.name );
    	}
        remote.set_account_seq(account.name, 1);

        var tx = remote.transaction();

        //console.log(account.balance);
        tx.payment(src, account.name, account.balance);

        tx.once('proposed', function (m) {
            //console.log('proposed: %s', JSON.stringify(m));
            callback(m.engine_result === 'tesSUCCESS' ? null : new Error());
        });

        tx.once('error', function (m) {
            console.log('error: %s', JSON.stringify(m));
            callback(m);
        });

        tx.submit();
    }, callback);
};

function credit_limit(remote, src, amount, callback) {
  assert(arguments.length === 4);

  var _m = amount.match(/^(\d+\/...\/[^\:]+)(?::(\d+)(?:,(\d+))?)?$/);

  if (!_m) {
    //console.log('credit_limit: parse error: %s', amount);
    return callback(new Error('parse_error'));
  }

   //console.log('credit_limit: parsed: %s', JSON.stringify(_m, undefined, 2));
  var account_limit = _m[1];
  var quality_in    = _m[2];
  var quality_out   = _m[3];

  var tx = remote.transaction()

  tx.ripple_line_set(src, account_limit, quality_in, quality_out)

  tx.once('proposed', function (m) {
     //console.log('proposed: %s', JSON.stringify(m));
    callback(m.engine_result === 'tesSUCCESS' ? null : new Error());
  });

  tx.once('error', function (m) {
    //console.log('error: %s', JSON.stringify(m));
    callback(m);
  });

  tx.submit();
};

function verify_limit(remote, src, amount, callback) {
  assert(arguments.length === 4);

  var _m = amount.match(/^(\d+\/...\/[^\:]+)(?::(\d+)(?:,(\d+))?)?$/);

  if (!_m) {
    // console.log('credit_limit: parse error: %s', amount);
    return callback(new Error('parse_error'));
  }

  // console.log('_m', _m.length, _m);
  // console.log('verify_limit: parsed: %s', JSON.stringify(_m, undefined, 2));
  var account_limit = _m[1];
  var quality_in    = Number(_m[2]);
  var quality_out   = Number(_m[3]);
  var limit         = Amount.from_json(account_limit);

  var options = {
    account:   src,
    issuer:    limit.issuer().to_json(),
    currency:  limit.currency().to_json(),
    ledger:    'CURRENT'
  };

  remote.request_ripple_balance(options, function(err, m) {
    if (err) {
      callback(err);
    } else {
      assert(m.account_limit.equals(limit));
      assert(isNaN(quality_in) || m.account_quality_in === quality_in);
      assert(isNaN(quality_out) || m.account_quality_out === quality_out);
      callback(null);
    }
  });
};

function credit_limits(remote, balances, callback) {
  assert(arguments.length === 3);

  var limits = [ ];

  for (var src in balances) {

    prepare_tests(balances[src], function(amount) {
      limits.push({
        source: src,
        amount: amount
      });
    });
  }

  function iterator(limit, callback) {
    credit_limit(remote, limit.source, limit.amount, callback);
  }

  async.some(limits, iterator, callback);
};

function ledger_close(remote, callback) {
  remote.once('ledger_closed', function (m) {
    callback();
  });
  remote.ledger_accept();
};

function payment(remote, src, dst, amount, callback) {
  assert(arguments.length === 5);

  //console.log( "payment src:%s  dst:%s", src, dst );
  remote.set_account_seq( src, 1 );
  var tx = remote.transaction();

  tx.payment(src, dst, amount);

  tx.once('proposed', function (m) {
    //console.log('proposed: %s', JSON.stringify(m));
    callback(m.engine_result === 'tesSUCCESS' ? null : new Error());
  });

  tx.once('error', function (m) {
    // console.log('error: %s', JSON.stringify(m));
    callback(m);
  });
  tx.submit();
};

function payments(remote, balances, callback) {
  assert(arguments.length === 3);

  var sends = [ ];

  for (var src in balances) {
    prepare_tests(balances[src], function(amount_json) {
      sends.push({
        source:        src,
        destination :  Amount.from_json(amount_json).issuer().to_json(),
        amount :       amount_json
      });
    });
  }

  function iterator(send, callback) {
    payment(remote, send.source, send.destination, send.amount, callback);
  }

  async.some(sends, iterator, callback);
};

function transfer_rate(remote, src, billionths, callback) {
  assert.strictEqual(arguments.length, 4);

  var tx = remote.transaction();
  tx.account_set(src);
  tx.transfer_rate(billionths);

  tx.once('proposed', function (m) {
    // console.log('proposed: %s', JSON.stringify(m));
    callback(m.engine_result === 'tesSUCCESS' ? null : new Error());
  });

  tx.once('error', function (m) {
    // console.log('error: %s', JSON.stringify(m));
    callback(m);
  });

  tx.submit();
};

function verify_balance(remote, src, amount_json, callback) {
  assert(arguments.length === 4);

  var amount_req  = Amount.from_json(amount_json);

  if (amount_req.is_native()) {
    remote.request_account_balance(src, 'CURRENT', function(err, amount_act) {
      if (err) {
        return callback(err);
      }
      var valid_balance = amount_act.equals(amount_req, true);
      if (!valid_balance) {
        console.log('verify_balance: failed: %s / %s',
        amount_act.to_text_full(),
        amount_req.to_text_full());
      }
      callback(valid_balance ? null : new Error());
    });
  } else {
    var issuer = amount_req.issuer().to_json();
    var currency = amount_req.currency().to_json();
    remote.request_ripple_balance(src, issuer, currency, 'CURRENT', function(err, m) {
      if (err) {
        return callback(err);
      }
      // console.log('BALANCE: %s', JSON.stringify(m));
      // console.log('account_balance: %s', m.account_balance.to_text_full());
      // console.log('account_limit: %s', m.account_limit.to_text_full());
      // console.log('issuer_balance: %s', m.issuer_balance.to_text_full());
      // console.log('issuer_limit: %s', m.issuer_limit.to_text_full());
      var account_balance = Amount.from_json(m.account_balance);

      var valid_balance = account_balance.equals(amount_req, true);

      if (!valid_balance) {
        console.log('verify_balance failed: %s) looking for: %s  found: %s',
        src,
        amount_req.to_text_full(),
        account_balance.to_text_full()
        );
      }

      callback(valid_balance ? null : new Error());
    })
  }
};

function verify_balances(remote, balances, callback) {
  var tests = [ ];

  for (var src in balances) {
    prepare_tests(balances[src], function(amount) {
      tests.push({ source: src, amount: amount });
    });
  }

  function iterator(test, callback) {
    verify_balance(remote, test.source, test.amount, callback)
  }

  async.each( tests, iterator, callback );

};

// --> owner: account
// --> seq: sequence number of creating transaction.
// --> taker_gets: json amount
// --> taker_pays: json amount
function verify_offer(remote, owner, seq, taker_pays, taker_gets, callback) {
  assert(arguments.length === 6);

  var request = remote.request_ledger_entry('offer')
  request.offer_id(owner, seq)
  request.callback(function(err, m) {
    var wrong = err
    || !Amount.from_json(m.node.TakerGets).equals(Amount.from_json(taker_gets), true)
    || !Amount.from_json(m.node.TakerPays).equals(Amount.from_json(taker_pays), true);

    if (wrong) {
      //console.log('verify_offer: failed: %s', JSON.stringify(m));
    }

    callback(wrong ? (err || new Error()) : null);
  });
};

function verify_offer_not_found(remote, owner, seq, callback) {
  assert(arguments.length === 4);

  var request = remote.request_ledger_entry('offer');

  request.offer_id(owner, seq);

  request.once('success', function (m) {
    //console.log('verify_offer_not_found: found offer: %s', JSON.stringify(m));
    callback(new Error('entryFound'));
  });

  request.once('error', function (m) {
    // console.log('verify_offer_not_found: success: %s', JSON.stringify(m));
    var is_not_found = m.error === 'remoteError' && m.remote.error === 'entryNotFound';
    if (is_not_found) {
      callback(null);
    } else {
      callback(new Error());
    }
  });

  request.request();
};

function verify_owner_count(remote, account, count, callback) {
  assert(arguments.length === 4);
  var options = { account: account, ledger: 'CURRENT' };
  remote.request_owner_count(options, function(err, owner_count) {
    //console.log('owner_count: %s/%d', owner_count, value);
    callback(owner_count === count ? null : new Error());
  });
};

function verify_owner_counts(remote, counts, callback) {
  var tests = prepare_tests(counts, function(account) {
    return { account: account, count: counts[account] };
  });

  function iterator(test, callback) {
    verify_owner_count(remote, test.account, test.count, callback)
  }

  async.each( tests, iterator, callback );
};

// takes an object or a string
function rpc(config,tx)
{
    if(typeof tx != 'string') tx=JSON.stringify(tx);

    return new Promise(function(resolve,reject){
        var uri='http://'+config.servers.alpha.rpc_ip+':'+config.servers.alpha.rpc_port;
        //console.log(uri);
        unirest.post(uri)
            .headers({ 'Accept': 'application/json' })
            .send(tx)
            .end(function (response) {

                if(response && response.body)
                {
                    if(response.body.result)
                    {
                        resolve(response.body.result);
                    }else
                    {
                        console.log("response: "+response.body);
                        //console.log(response);
                        reject("Error during RPC");
                    }
                }else
                {
                    console.log("no response?: "+tx+JSON.stringify(response));
                    //console.log(response);
                    reject("Error during RPC");
                }
            });
    });
}

function custom_ledger_helper(t, remote, precallback, callback) {
	var res = function ( cb ) {
		if ( precallback ) {
			precallback();
		}
		remote.request_ledger( 'current', true )
		.on( 'success', function ( m ) {
			callback(m, cb );
		} ).on( 'error', function ( m ) {
			callback(new Error(m), cb);
		} ).request();
	};
	return res;
}

function display_ledger_helper( t, remote, description ) {
	var res = custom_ledger_helper(t, remote, function() {
			t.what = "Display ledger " + description;
			console.log( "---------------------" + description + "-------------------" );
		},
		function ( ledger, callback ) {
			console.log( "Ledger: %s", JSON.stringify( ledger, undefined, 2 ) );
			callback();
		}
	);
	return res;
}


exports.account_dump            = account_dump;
exports.build_setup             = build_setup;
exports.build_teardown          = build_teardown;
exports.create_accounts         = create_accounts;
exports.createAccountsFromObjects         = createAccountsFromObjects;
exports.credit_limit            = credit_limit;
exports.credit_limits           = credit_limits;
exports.get_config              = get_config;
exports.init_config             = init_config;
exports.ledger_close            = ledger_close;
exports.payment                 = payment;
exports.payments                = payments;
exports.transfer_rate           = transfer_rate;
exports.verify_balance          = verify_balance;
exports.verify_balances         = verify_balances;
exports.verify_limit            = verify_limit;
exports.verify_offer            = verify_offer;
exports.verify_offer_not_found  = verify_offer_not_found;
exports.verify_owner_count      = verify_owner_count;
exports.verify_owner_counts     = verify_owner_counts;
exports.rpc                     = rpc;
exports.display_ledger_helper   = display_ledger_helper;
exports.custom_ledger_helper    = custom_ledger_helper;

process.on('uncaughtException', function() {
  Object.keys(server).forEach(function(host) {
    server[host].stop();
  });
});

// vim:sw=2:sts=2:ts=8:et
