var async     = require("async");
var assert    = require('assert');
var Remote    = require("stellar-lib").Remote;
var testutils = require("./testutils");
var unirest   = require('unirest');
var config    = testutils.init_config();

suite('Account set', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test('set InflationDest', function(done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = "Set InflationDest.";

          //console.log(config.accounts.root.account);

        $.remote.transaction()
        .account_set(config.accounts.root.account)
		.inflation_dest(config.accounts.root.account)
        .on('submitted', function (m) {
          //console.log("proposed: %s", JSON.stringify(m));
            testutils.auto_advance_default( $.remote, m, callback );
        })
        .submit();
      }
    ]

    async.waterfall(steps,function (error) {
      assert(!error, self.what);
      done();
    });
  });


    test('set SetAuthKey', function(done) {
        var self = this;

        var steps = [
            function (callback) {
                self.what = "Create account";
                testutils.create_accounts($.remote, "root", "10000.0", ["alice"], callback);
            },
            function (callback) {
                self.what = "Set auth required flag";

                //console.log(config.accounts.root.account);

                $.remote.transaction()
                    .account_set(config.accounts.root.account)
                    .set_flags('RequireAuth')
                    .on('submitted', function (m) {
                        //console.log("result: %s", JSON.stringify(m));
                        testutils.auto_advance_default( $.remote, m, callback );
                    })
                    .submit();
            },
            function ( callback ) {
                self.what = "Try to set credit limit";

                testutils.credit_limits($.remote, {
                        "alice" : [ "100/USD/root" ]
                    },
                    callback);
            },

            function (callback) {
                self.what = "Set SetAuthKey.";

                //console.log(config.accounts.root.account);

                $.remote.transaction()
                    .account_set(config.accounts.root.account)
                    .set_auth_key(config.accounts.mtgox.account)
                    .on('submitted', function (m) {
                        //console.log("result: %s", JSON.stringify(m));
                        testutils.auto_advance_default( $.remote, m, callback );
                    })
                    .submit();
            },

            function (callback) {
                self.what = "hot wallet Auths holding.";

                //console.log(config.accounts.root.account);

                //we need to be able to sign this tx with the mtgox key even though the src is a different account

                var tx=$.remote.transaction()
                    .ripple_line_set(config.accounts.root.account, "0/USD/alice")
                    .set_flags('SetAuth')
                    .on('submitted', function (m) {
                        //console.log("result: %s", JSON.stringify(m));
                        testutils.auto_advance_default( $.remote, m, callback );
                    });
                tx._secret=config.accounts.mtgox.secret;
                tx.tx_json.Sequence=4;
                tx.complete();
                //console.log("tx_json: ",tx.tx_json);
                tx.sign();
                tx._secret=null;
                tx.submit();

            },

            function ( callback ) {
                self.what = "send payment";

                var tx=$.remote.transaction()
                    .payment('root', 'alice', "24/USD/root")
                    .once('submitted', function (m) {
                        console.log("proposed: %s", JSON.stringify(m));
                        testutils.auto_advance_default( $.remote, m, callback );
                    });

                tx.tx_json.Sequence=5;
                tx.complete();
                tx.submit();
            },

            function ( callback ) {
                self.what = "Set credit limit";

                testutils.credit_limits($.remote, {
                        "alice" : [ "200/USD/root" ]
                    },
                    callback);
            }
        ]

        async.waterfall(steps,function (error) {
            assert(!error, self.what);
            done();
        });
    });

  
  test('set RequireDestTag', function(done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = "Set RequireDestTag.";

        $.remote.transaction()
        .account_set("root")
        .set_flags('RequireDestTag')
        .on('submitted', function (m) {
          //console.log("proposed: %s", JSON.stringify(m));
            testutils.auto_advance_default( $.remote, m, callback );
        })
        .submit();
      },

      function (callback) {
        self.what = "Check RequireDestTag";

        $.remote.request_account_flags('root', 'CURRENT')
        .on('success', function (m) {
          var wrong = !(m.node.Flags & Remote.flags.account_root.RequireDestTag);

          if (wrong) {
            console.log("Set RequireDestTag: failed: %s", JSON.stringify(m));
          }

          callback(wrong ? new Error(wrong) : null);
        })
        .request();
      },

      function (callback) {
        self.what = "Clear RequireDestTag.";

        $.remote.transaction()
        .account_set("root")
        .set_flags('OptionalDestTag')
        .on('submitted', function (m) {
          //console.log("proposed: %s", JSON.stringify(m));
            testutils.auto_advance_default( $.remote, m, callback );
        })
        .submit();
      },

      function (callback) {
        self.what = "Check No RequireDestTag";

        $.remote.request_account_flags('root', 'CURRENT')
        .on('success', function (m) {
          var wrong = !!(m.node.Flags & Remote.flags.account_root.RequireDestTag);

          if (wrong) {
            console.log("Clear RequireDestTag: failed: %s", JSON.stringify(m));
          }

          callback(wrong ? new Error(m) : null);
        })
        .request();
      }
    ]

    async.waterfall(steps,function (error) {
      assert(!error, self.what);
      done();
    });
  });

  test("set RequireAuth",  function (done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = "Set RequireAuth.";

        $.remote.transaction()
        .account_set("root")
        .set_flags('RequireAuth')
        .on('submitted', function (m) {
          //console.log("proposed: %s", JSON.stringify(m));
            testutils.auto_advance_default( $.remote, m, callback );
        })
        .submit();
      },

      function (callback) {
        self.what = "Check RequireAuth";

        $.remote.request_account_flags('root', 'CURRENT')
        .on('error', callback)
        .on('success', function (m) {
          var wrong = !(m.node.Flags & Remote.flags.account_root.RequireAuth);

          if (wrong) {
            console.log("Set RequireAuth: failed: %s", JSON.stringify(m));
          }

          callback(wrong ? new Error(m) : null);
        })
        .request();
      },

      function (callback) {
        self.what = "Clear RequireAuth.";

        $.remote.transaction()
        .account_set("root")
        .set_flags('OptionalAuth')
        .on('submitted', function (m) {
          //console.log("proposed: %s", JSON.stringify(m));

            testutils.auto_advance_default( $.remote, m, callback );
        })
        .submit();
      },

      function (callback) {
        self.what = "Check No RequireAuth";

        $.remote.request_account_flags('root', 'CURRENT')
        .on('error', callback)
        .on('success', function (m) {
          var wrong = !!(m.node.Flags & Remote.flags.account_root.RequireAuth);

          if (wrong) {
            console.log("Clear RequireAuth: failed: %s", JSON.stringify(m));
          }

          callback(wrong ? new Error(m) : null);
        })
        .request();
      }
      // XXX Also check fails if something is owned.
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });

});
