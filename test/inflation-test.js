var async     = require("async");
var assert    = require('assert');
var bigint    = require("BigInt");
var Amount    = require("stellar-lib").Amount;
var Remote    = require("stellar-lib").Remote;
var Server    = require("./server").Server;
var testutils = require("./testutils");
var config    = testutils.init_config();

/*
 Need to test:

 Voting for deleted accounts
 When no one gets over the min %
 When there is no fee to distribute
 A inflation transaction introduced out of seq
 A inflation transaction introduced too soon
 Two inflation transactions make it into the same ledger
 Test load of a ledger with a million accounts all voting for different people. How much time does it add to ledger close
 */



suite('Inflation', function() {
    var $ = { };
    var DUST_MULTIPLIER= 1000000;
    var TOTAL_COINS = 100000000000;
    var INFLATION_RATE = 0.000190721;
    var INFLATION_ROUND = DUST_MULTIPLIER * TOTAL_COINS * INFLATION_RATE;
    var INT_SIZE_BITS = 256;

    setup(function(done) {
        testutils.build_setup().call($, done);
    });

    teardown(function(done) {
        testutils.build_teardown().call($, done);
    });

    // Two guys get over threshold
    test('Inflation #4 - two guys over threshold', function(done) {
        var self = this;
        var start_balance = 10000000000;
        var tx_fee = 12; //TODO: get tx fee


        var accountObjects=[];
        for(var n=0; n<12; n++)
        {
            if(n<6)var next=0;
            else if(n==6) var next=2;
            else if(n==7) var next=3;
            else var next=1;

            var bnBalance = bigint.int2bigInt((n+1)*100000000, INT_SIZE_BITS, 0);
            bnBalance = bigint.mult(bnBalance, bigint.int2bigInt(DUST_MULTIPLIER, INT_SIZE_BITS, 0));
            var bnTargetBalance = bigint.sub(bnBalance,bigint.int2bigInt(tx_fee, INT_SIZE_BITS, 0));

            accountObjects[n]={ name:''+n, targetBalance: bnTargetBalance , voteFor:''+next};
            accountObjects[n].balance=''+bigint.bigInt2str(bnBalance,10);
            //console.log("total:"+bigint.bigInt2str(accountObjects[n].targetBalance,10));
            accountObjects[n].votes = bigint.int2bigInt(0,INT_SIZE_BITS,0);
        }


        for(var n=0; n<12; n++)
        {
            var votes = accountObjects[ accountObjects[n].voteFor ].votes;
            votes = bigint.add(accountObjects[n].targetBalance, votes);
            accountObjects[ accountObjects[n].voteFor ].votes = votes;
        }

        var winBasis=  bigint.add(accountObjects[0].votes, accountObjects[1].votes);

        var totalFees = tx_fee*24;
        var coinsToDole = (DUST_MULTIPLIER * TOTAL_COINS - totalFees) * INFLATION_RATE;
        var prizePool= bigint.str2bigInt( Math.floor(coinsToDole+totalFees).toString(), 10);


        var prod = bigint.mult(accountObjects[0].votes, prizePool);
        var res  = bigint.dup(prod);
        var reminder = bigint.dup(prod);

        bigint.divide_( prod , winBasis, res, reminder);
        accountObjects[0].targetBalance = bigint.add(accountObjects[0].targetBalance, res);
        console.log("div0: "+bigint.bigInt2str(res,10));



        prod = bigint.mult(accountObjects[1].votes, prizePool);
        res  = bigint.dup(prod);
        reminder = bigint.dup(prod);

        console.log("prod:"+bigint.bigInt2str(prod,10));
        console.log("winBasis:"+bigint.bigInt2str(winBasis,10));

        bigint.divide_( prod , winBasis, res, reminder);
        console.log("div1: "+bigint.bigInt2str(res,10));
        accountObjects[1].targetBalance = bigint.add(accountObjects[1].targetBalance, res);



        var steps = [

            function (callback) {
                self.what = "Create accounts.";
                testutils.createAccountsFromObjects($.remote, "root", accountObjects, callback);
            },

            function (wfCB) {
                self.what = "Set InflationDests";

                async.eachSeries(accountObjects, function (account, callback) {
                    $.remote.transaction()
                        .account_set(account.name)
                        .inflation_dest($.remote.account(account.voteFor)._account_id)
                        .on('submitted', function (m) {
                            if (m.engine_result === 'tesSUCCESS') {
                                $.remote.ledger_accept();    // Move it along.
                                callback(null);
                            } else {
                                console.log('',m);
                                callback(new Error(m.engine_result));
                            }
                        })
                        .submit();
                }, wfCB);
            },

            function (callback) {
                self.what = "Do inflation";

                console.log('INFLATE');
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

            function (wfCB) {
                self.what = "Check all balances";

                async.eachSeries(accountObjects, function (account, callback) {
                    $.remote.requestAccountBalance($.remote.account(account.name)._account_id, 'current', null)
                        .on('success', function (m) {

                            var balance = Number( bigint.bigInt2str(account.targetBalance,10));
                            //console.log("num:"+bigint.bigInt2str(account.targetBalance,10));
                            console.log('target balance('+account.name+'): '+balance/DUST_MULTIPLIER+' vs '+m.node.Balance/DUST_MULTIPLIER);
                            //console.log('target votes('+account.name+'): '+account.votes/DUST_MULTIPLIER+' vs '+m.node.Balance/DUST_MULTIPLIER);


                            // TODO: a bit lame but will be annoying to get the libraries to produce the exact same result
                            //assert( Math.abs((m.node.Balance - account.targetBalance)/account.targetBalance) <.0001 );
                            assert(balance==m.node.Balance);
                            callback();
                        }).request();
                },wfCB);
            }
        ]

        async.waterfall(steps,function (error) {
            assert(!error, self.what);
            done();
        });
    });

    // When no one gets over the min %
    // so choose the top 6 guys
    test('Inflation #3 - no one over min', function(done) {
        var self = this;
        var start_balance = 10000000000;
        var tx_fee = 12; //TODO: get tx fee

        var accountObjects=[];
        for(var n=0; n<12; n++)
        {
            accountObjects[n]={ name:''+n, targetBalance: (DUST_MULTIPLIER*1000*(n+1)), voteFor:''+(n+1)};
            accountObjects[n].balance=''+accountObjects[n].targetBalance;
            accountObjects[n].targetBalance -= tx_fee;
        }

        var prizePool=INFLATION_ROUND+tx_fee*24;
        console.log("pp: "+prizePool);
        var winBasis=accountObjects[6].targetBalance+accountObjects[11].targetBalance+accountObjects[10].targetBalance+
            accountObjects[9].targetBalance+accountObjects[8].targetBalance+accountObjects[7].targetBalance+
            accountObjects[5].targetBalance;
/*
        console.log("winBasis: "+winBasis);
        var pp=(accountObjects[11].targetBalance/winBasis)*prizePool+(accountObjects[6].targetBalance/winBasis)*prizePool
            +(accountObjects[7].targetBalance/winBasis)*prizePool+(accountObjects[8].targetBalance/winBasis)*prizePool
            +(accountObjects[9].targetBalance/winBasis)*prizePool+(accountObjects[10].targetBalance/winBasis)*prizePool;

        console.log("pp: "+pp);
*/
        accountObjects[0].targetBalance=accountObjects[0].targetBalance+(accountObjects[11].targetBalance/winBasis)*prizePool;
        accountObjects[11].targetBalance=accountObjects[11].targetBalance+(accountObjects[10].targetBalance/winBasis)*prizePool;
        accountObjects[10].targetBalance=accountObjects[10].targetBalance+(accountObjects[9].targetBalance/winBasis)*prizePool;
        accountObjects[9].targetBalance=accountObjects[9].targetBalance+(accountObjects[8].targetBalance/winBasis)*prizePool;
        accountObjects[8].targetBalance=accountObjects[8].targetBalance+(accountObjects[7].targetBalance/winBasis)*prizePool;
        accountObjects[7].targetBalance=accountObjects[7].targetBalance+(accountObjects[6].targetBalance/winBasis)*prizePool;
        accountObjects[6].targetBalance=accountObjects[6].targetBalance+(accountObjects[5].targetBalance/winBasis)*prizePool;
/*
        console.log("0: "+accountObjects[0].targetBalance);
        console.log("7: "+accountObjects[7].targetBalance);
        console.log("8: "+accountObjects[8].targetBalance);
        console.log("9: "+accountObjects[9].targetBalance);
        console.log("10: "+accountObjects[10].targetBalance);
        console.log("11: "+accountObjects[11].targetBalance);



        console.log("sum balance:"+(accountObjects[0].targetBalance+accountObjects[7].targetBalance+accountObjects[8].targetBalance+accountObjects[9].targetBalance+accountObjects[10].targetBalance+accountObjects[11].targetBalance));
*/
        accountObjects[11].voteFor='0';

        var steps = [

            function (callback) {
                self.what = "Create accounts.";
                testutils.createAccountsFromObjects($.remote, "root", accountObjects, callback);
            },

            function (wfCB) {
                self.what = "Set InflationDests";

                async.eachSeries(accountObjects, function (account, callback) {
                    $.remote.transaction()
                        .account_set(account.name)
                        .inflation_dest($.remote.account(account.voteFor)._account_id)
                        .on('submitted', function (m) {
                            if (m.engine_result === 'tesSUCCESS') {
                                $.remote.ledger_accept();    // Move it along.
                                callback(null);
                            } else {
                                console.log('',m);
                                callback(new Error(m.engine_result));
                            }
                        })
                        .submit();
                }, wfCB);
            },

            function (callback) {
                self.what = "Do inflation";

                console.log('INFLATE');
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

            function (wfCB) {
                self.what = "Check all balances";

                async.eachSeries(accountObjects, function (account, callback) {
                    $.remote.requestAccountBalance($.remote.account(account.name)._account_id, 'current', null)
                        .on('success', function (m) {


                            console.log('target('+account.name+'): '+account.targetBalance+' vs '+m.node.Balance);

                            // TODO: a bit lame but will be annoying to get the libraries to produce the exact same result
                            assert( Math.abs((m.node.Balance - account.targetBalance)/account.targetBalance) <.0001 );
                            callback();
                        }).request();
                },wfCB);
            }
        ]

        async.waterfall(steps,function (error) {
            assert(!error, self.what);
            done();
        });
    });

/*
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
*/


});

// vim:sw=2:sts=2:ts=8:et
