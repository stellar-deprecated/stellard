var async     = require("async");
var assert    = require('assert');
var bigint    = require("BigInt");
var Amount    = require("stellar-lib").Amount;
var Remote    = require("stellar-lib").Remote;
var Request    = require("stellar-lib").Request;
var Server    = require("./server").Server;
var testutils = require("./testutils");
var config    = testutils.init_config();

/*
 Need to test:

 -Voting for deleted accounts
 +When no one gets over the min %
 +When there is no fee to distribute
 +A inflation transaction introduced out of seq
 A inflation transaction introduced too soon
 +Two inflation transactions make it into the same ledger
 Test load of a ledger with a million accounts all voting for different people. How much time does it add to ledger close
 */



suite('Inflation', function() {
    var $ = { };
    var DUST_MULTIPLIER= 1000000;
    var TOTAL_COINS = 100000000000;
    var INFLATION_RATE = 0.000190721;
    var INFLATION_WIN_MIN_PERMIL = 15;
    var INFLATION_WIN_MIN_VOTES = TOTAL_COINS / 1000 * INFLATION_WIN_MIN_PERMIL;
    var INFLATION_NUM_WINNERS = 50;
    var INT_SIZE_BITS = 256;

    // calculate balances after inflation, returns total doled coins
    function calcInflation(accountObjects, accNum, pool_fee, total_coins){

        for(var n=0; n<accNum; n++)
            accountObjects[n].votes = 0;

        for ( var n = 0; n < accNum; n++ )
        {
        	accountObjects[accountObjects[n].voteForIndex].votes += parseInt( bigint.bigInt2str( accountObjects[n].targetBalance, 10 ) );
        }

        var winningAccountsNum = 0;
        var winningAccounts = [];
        var minVotes = INFLATION_WIN_MIN_VOTES*DUST_MULTIPLIER;

        var sortedAccounts = accountObjects.slice().sort( function(a,b){return b.votes - a.votes} );

        for(var n=0; n<accNum && winningAccountsNum<INFLATION_NUM_WINNERS; n++){
            if( sortedAccounts[n].votes > minVotes ) {
                winningAccounts[winningAccountsNum++] = sortedAccounts[n];
            }
            else
                break;
        }


        //no account over min threshold
        if(winningAccountsNum == 0)
            for(var n=0; n<accNum && winningAccountsNum<INFLATION_NUM_WINNERS; n++){
                winningAccounts[winningAccountsNum++] = sortedAccounts[n];
            }

        //console.log("winning accounts: "+winningAccountsNum);

        var winBasis = 0;

        for(var n=0; n<winningAccountsNum; n++){
            winBasis += winningAccounts[n].votes;
        }

        var bnWinBasis = bigint.str2bigInt(winBasis.toString(), 10);
        //var totalFees = tx_fee*accNum*2;
        var coinsToDole = ( DUST_MULTIPLIER * total_coins - pool_fee ) * INFLATION_RATE;
        var prizePool = bigint.str2bigInt( Math.floor( coinsToDole + pool_fee ).toString(), 10 );
        var totalDoledCoins = bigint.int2bigInt(0, INT_SIZE_BITS, 0);

        //console.log( "  total_coins: " + total_coins + "  poolFee:" + pool_fee + "  toDole:" + Math.floor( coinsToDole + pool_fee ).toString() );

        for(var n=0; n<winningAccountsNum; n++){
            var votes = bigint.str2bigInt( winningAccounts[n].votes.toString(), 10 );
            var prod = bigint.mult( votes, prizePool);
            var res  = bigint.dup(prod);
            var reminder = bigint.dup(prod);

            bigint.divide_( prod , bnWinBasis, res, reminder);
            winningAccounts[n].targetBalance = bigint.add(winningAccounts[n].targetBalance, res);
            totalDoledCoins = bigint.add(totalDoledCoins, res);
        }

        return parseInt( bigint.bigInt2str(totalDoledCoins,10) )/DUST_MULTIPLIER;
    }

	// calculates the coin increase based on the size of the fee pool
    function newCoinsFromInflation(fee_pool) {
    	return (DUST_MULTIPLIER * (TOTAL_COINS) - fee_pool ) * INFLATION_RATE + fee_pool;
    }

    // calculate balances for additional (non-initial) inflations
    function doSubsequentInflation( accNum, accountObjects, pool_fee, total_coins )
    {

        for(var n=0; n<accNum; n++)
        {
            accountObjects[n].balance = ''+bigint.bigInt2str(accountObjects[n].targetBalance,10);
        }

        return total_coins + calcInflation( accountObjects, accNum, pool_fee, total_coins ) - pool_fee / DUST_MULTIPLIER;
    }

    // make initial test accounts, calculate first inflation
    function makeTestAccounts(accNum, accBalanceFun, accVoteFun, tx_fee){

        var accountObjects=[];
        for(var n=0; n<accNum; n++)
        {
            var bnBalance = bigint.int2bigInt(accBalanceFun(n), INT_SIZE_BITS, 0);
            bnBalance = bigint.mult(bnBalance, bigint.int2bigInt(DUST_MULTIPLIER, INT_SIZE_BITS, 0));
            var bnTargetBalance = bigint.sub(bnBalance,bigint.int2bigInt(tx_fee, INT_SIZE_BITS, 0));

            accountObjects[n] = { name: 'A' + n, targetBalance: bnTargetBalance, voteForIndex: accVoteFun( n ) };
            accountObjects[n].voteForName = 'A' + accVoteFun( n );
            accountObjects[n].balance=''+bigint.bigInt2str(bnBalance,10);
            //accountObjects[n].balanceInt=parseInt(accountObjects[n].balance);
            accountObjects[n].votes = 0;
        }


        var totalFees = tx_fee*accNum*2;
        var doledCoins = calcInflation(accountObjects, accNum, totalFees, TOTAL_COINS);
        var newTotalCoins = TOTAL_COINS + doledCoins - totalFees/DUST_MULTIPLIER;

        return {accountObjects:accountObjects, totalCoins: newTotalCoins};
    };

    setup(function(done) {
        testutils.build_setup().call($, done);
    });

    teardown(function(done) {
        testutils.build_teardown().call($, done);
    });

    function inflation(seq) {
    	var $t = $.remote.transaction();
    	$t.tx_json.Fee = 0; // ugly, force fee for inflation to 0
    	$t.inflation($.remote.account('root')._account_id, seq);
    	return $t;
    }

    test('Inflation #1 - two guys over threshold', function(done) {
        var self = this;
        var tx_fee = 12; //TODO: get tx fee

        var voteFunc = function(n){
            if(n<6) return 0;
            else if(n==6) return 2;
            else if(n==7) return 3;
            else return 1;
        };

        var balanceFunc = function(n){
            return (n+1)*100000000;
        };

        var accountObjects= makeTestAccounts(12, balanceFunc, voteFunc, tx_fee).accountObjects;
        var failed = false;

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
                        .inflation_dest($.remote.account(account.voteForName)._account_id)
                        .on('submitted', function (m) {
                            if (m.engine_result === 'tesSUCCESS') {
                                $.remote.once('ledger_closed', function(ledger_closed, ledger_index) { callback(); } );
                                $.remote.ledger_accept();    // Move it along.

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

                //console.log('INFLATE');
                inflation(1)
                    .on('submitted', function (m) {
                        if (m.engine_result === 'tesSUCCESS') {
                            $.remote.once('ledger_closed', function(ledger_closed, ledger_index) { callback(); } );
                            $.remote.ledger_accept();    // Move it along.

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

                            if(balance!=m.node.Balance) failed = true;

                            if(failed)
                                console.log('target balance('+account.name+'): '+balance/DUST_MULTIPLIER+' vs '+m.node.Balance/DUST_MULTIPLIER);

                            //assert(balance==m.node.Balance);
                            callback();
                        }).request();
                },wfCB);
            },

            function (wfCB) {
                self.what = "Check all balances / fail check";

                assert(failed==false);

                wfCB();
            }
        ]

        async.waterfall(steps,function (error) {
            assert(!error, self.what);
            done();
        });
    });

    // When no one gets over the min %
    // so choose the top 50 guys (or all 12 in this case)
    test('Inflation #2 - no one over min', function(done) {
        var self = this;
        var tx_fee = 12; //TODO: get tx fee

        var voteFun = function(n){
            if(n==11) return 0;
            return n+1;
        };

        var balanceFun = function(n){
            return (n+1)*1000;
        };

        var accountObjects=makeTestAccounts(12, balanceFun, voteFun, tx_fee).accountObjects;
        var failed = false;

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
                        .inflation_dest($.remote.account(account.voteForName)._account_id)
                        .on('submitted', function (m) {
                            if (m.engine_result === 'tesSUCCESS') {
                                $.remote.once('ledger_closed', function(ledger_closed, ledger_index) { callback(); } );
                                $.remote.ledger_accept();    // Move it along.
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

                inflation(1)
                    .on( 'submitted', function ( m )
                    {
                        if (m.engine_result === 'tesSUCCESS') {
                            $.remote.once('ledger_closed', function(ledger_closed, ledger_index) { callback(); } );
                            $.remote.ledger_accept();    // Move it along.
                        } else
                        {
                        	console.log( 'Inflation error %s', JSON.stringify( m.engine_result ) );
                            callback(new Error(m.engine_result));
                        }
                    })
                    .on( 'error', function ( m )
                    {
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
                            if(m.node.Balance != balance) failed = true;

                            if(failed)
                                console.log('target('+account.name+'): '+balance/DUST_MULTIPLIER+' vs '+m.node.Balance/DUST_MULTIPLIER);

                            //assert(m.node.Balance == balance);
                            callback();
                        }).request();
                },wfCB);
            },

            function (wfCB) {
                self.what = "Check all balances / fail check";

                assert(failed==false);

                wfCB();
            }
        ]

        async.waterfall(steps,function (error) {
            assert(!error, self.what);
            done();
        });
    });
	
    // Using two consecutive inflations, no fee for the second inflation
    test('Inflation #3 - No fees to distribute', function(done) {
        var self = this;
        var tx_fee = 12; //TODO: get tx fee

        var voteFunc = function(n){
            if(n<6) return 0;
            else if(n==6) return 2;
            else if(n==7) return 3;
            else return 1;
        };

        var balanceFunc = function(n){
            return (n+1)*100000000;
        };

        var accData = makeTestAccounts(12, balanceFunc, voteFunc, tx_fee);
        var accountObjects= accData.accountObjects;
        var totalCoins = accData.totalCoins;
        var failed = false;

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
                        .inflation_dest($.remote.account(account.voteForName)._account_id)
                        .on('submitted', function (m) {
                            if (m.engine_result === 'tesSUCCESS') {
                                $.remote.once('ledger_closed', function(ledger_closed, ledger_index) { callback(); } );
                                $.remote.ledger_accept();    // Move it along.
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

                //console.log('INFLATE');
                inflation(1)
                    .on('submitted', function (m) {
                        if (m.engine_result === 'tesSUCCESS') {
                            $.remote.once('ledger_closed', function(ledger_closed, ledger_index) { callback(); } );
                            $.remote.ledger_accept();    // Move it along.
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
                            if(balance!=m.node.Balance)
                                failed = true;

                            if(failed)
                                console.log('target balance('+account.name+'): '+balance/DUST_MULTIPLIER+' vs '+m.node.Balance/DUST_MULTIPLIER);

                            //assert(balance==m.node.Balance);
                            callback();
                        }).request();
                },wfCB);
            },

            function (wfCB) {
                self.what = "Check all balances / fail check";

                assert(failed==false);

                wfCB();
            },

            function (callback) {
                self.what = "Do inflation #2";

				// no transaction since last inflation -> fee_pool == 0
                totalCoins = doSubsequentInflation(12, accountObjects, 0 , totalCoins);

                //console.log('INFLATE #2');
                inflation(2)
                    .on('submitted', function (m) {
                        if (m.engine_result === 'tesSUCCESS') {
                            $.remote.once('ledger_closed', function(ledger_closed, ledger_index) { callback(); } );
                            $.remote.ledger_accept();    // Move it along.
                        } else {
                            console.log('error: %s', JSON.stringify(m));
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
                self.what = "Check all balances #2";

                //console.log( 'Check all balances #2');
                async.eachSeries(accountObjects, function (account, callback) {
                    $.remote.requestAccountBalance($.remote.account(account.name)._account_id, 'current', null)
                        .on('success', function (m) {

                            var balance = Number( bigint.bigInt2str(account.targetBalance,10));
                            if(balance!=m.node.Balance)
                                failed = true;

                            if(failed)
                                console.log('target balance('+account.name+'): '+balance/DUST_MULTIPLIER+' vs '+m.node.Balance/DUST_MULTIPLIER);

                            //assert(balance==m.node.Balance);
                            callback();
                        }).request();
                },wfCB);
            },

            function (wfCB) {
                self.what = "Check all balances #2 / fail check";

                assert(failed==false);

                wfCB();
            }
        ]

        async.waterfall(steps,function (error) {
            assert(!error, self.what);
            done();
        });
    });
	
    // Use wrong sequence ID
    test('Inflation #4 - wrong sequence ID', function(done) {
        var self = this;
        var tx_fee = 12; //TODO: get tx fee

        var voteFunc = function(n){
            if(n<6) return 0;
            else if(n==6) return 2;
            else if(n==7) return 3;
            else return 1;
        };

        var balanceFunc = function(n){
            return (n+1)*100000000;
        };

        var accountObjects=makeTestAccounts(12, balanceFunc, voteFunc, tx_fee).accountObjects;


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
                        .inflation_dest($.remote.account(account.voteForName)._account_id)
                        .on('submitted', function (m) {
                            if (m.engine_result === 'tesSUCCESS') {
                                $.remote.once('ledger_closed', function(ledger_closed, ledger_index) { callback(); } );
                                $.remote.ledger_accept();    // Move it along.
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

                //console.log('INFLATE');
                inflation(2) //bad sequence ID
                    .on('submitted', function (m) {

                        if (m.engine_result === 'tesSUCCESS') {
                            callback(new Error("Inflation succedded with wrong sequence ID"));
                        } else {
                            callback(null);
                        }
                    })
                    .submit();
            }
        ]

        async.waterfall(steps,function (error) {
            assert(!error, self.what);
            done();
        });
    });

    test( 'Inflation #5 - Two inflation transactions make it into the same ledger, same seq ID', function ( done ) {
        var self = this;
        var tx_fee = 12; //TODO: get tx fee

        var voteFunc = function(n){
            if(n<6) return 0;
            else if(n==6) return 2;
            else if(n==7) return 3;
            else return 1;
        };

        var balanceFunc = function(n){
            return (n+1)*100000000;
        };

        var accData = makeTestAccounts(12, balanceFunc, voteFunc, tx_fee);
        var accountObjects= accData.accountObjects;
        var totalCoins = accData.totalCoins;
        var failed = false;

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
                        .inflation_dest($.remote.account(account.voteForName)._account_id)
                        .on('submitted', function (m) {
                            if (m.engine_result === 'tesSUCCESS') {
                                $.remote.once('ledger_closed', function(ledger_closed, ledger_index) { callback(); } );
                                $.remote.ledger_accept();    // Move it along.
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

                //console.log('INFLATE');
                inflation(1)
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


            function (wfCB) {
                self.what = "Check all balances";

                async.eachSeries(accountObjects, function (account, callback) {
                    $.remote.requestAccountBalance($.remote.account(account.name)._account_id, 'current', null)
                        .on('success', function (m) {

                            var balance = Number( bigint.bigInt2str(account.targetBalance,10));
                            if(balance!=m.node.Balance)
                                failed = true;

                            if(failed)
                                console.log('target balance('+account.name+'): '+balance/DUST_MULTIPLIER+' vs '+m.node.Balance/DUST_MULTIPLIER);

                            //assert(balance==m.node.Balance);
                            callback();
                        }).request();
                },wfCB);
            },

            function (wfCB) {
                self.what = "Check all balances / fail check";

                assert(failed==false);

                wfCB();
            },

            function (callback) {
                self.what = "Do inflation #2";

                totalCoins = doSubsequentInflation(12, accountObjects, 12, totalCoins);

                //console.log('INFLATE #2');
                inflation(1)
                    .on('submitted', function (m) {
                        if (m.engine_result === 'tesSUCCESS') {
                            callback(new Error("Inflation succedded with wrong sequence ID"));
                        } else {
                            callback(null);
                        }
                    })
                    .submit();
            }
        ]

        async.waterfall(steps,function (error) {
            assert(!error, self.what);
            done();
        });
    });

    test('Inflation #5.1 - Two inflation transactions make it into the same ledger, different seq ID (not working right now)', function(done) {
        var self = this;
        var tx_fee = 12; //TODO: get tx fee

        var voteFunc = function(n){
            if(n<6) return 0;
            else if(n==6) return 2;
            else if(n==7) return 3;
            else return 1;
        };

        var balanceFunc = function(n){
            return (n+1)*100000000;
        };

        var accData = makeTestAccounts(12, balanceFunc, voteFunc, tx_fee);
        var accountObjects= accData.accountObjects;
        var totalCoins = accData.totalCoins;
        var failed = false;

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
                        .inflation_dest($.remote.account(account.voteForName)._account_id)
                        .on('submitted', function (m) {
                            if (m.engine_result === 'tesSUCCESS') {
                            	$.remote.once( 'ledger_closed', function ( ledger_closed, ledger_index ) { callback(); } );
                                $.remote.ledger_accept();    // Move it along.
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

                //console.log('INFLATE');
                inflation(1)
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


            function (wfCB) {
                self.what = "Check all balances";

                async.eachSeries(accountObjects, function (account, callback) {
                    $.remote.requestAccountBalance($.remote.account(account.name)._account_id, 'current', null)
                        .on('success', function (m) {

                            var balance = Number( bigint.bigInt2str(account.targetBalance,10));
                            if(balance!=m.node.Balance)
                                failed = true;

                            if(failed)
                                console.log('target balance('+account.name+'): '+balance/DUST_MULTIPLIER+' vs '+m.node.Balance/DUST_MULTIPLIER);

                            //assert(balance==m.node.Balance);
                            callback();
                        }).request();
                },wfCB);
            },

            function (wfCB) {
                self.what = "Check all balances / fail check";

                assert(failed==false);

                wfCB();
            },

            function (callback) {
                self.what = "Do inflation #2";

                //console.log( 'INFLATE #2' );

				// fee_pool == 0 as we didn't do anything since the last time
                totalCoins = doSubsequentInflation(12, accountObjects, 0, totalCoins);
                inflation(2)
                    .on( 'submitted', function ( m ) {
                    	if ( m.engine_result === 'telNOT_TIME' ) { // we expect a failure
                    		callback(null);
                    	} else {
                    		callback( new Error( "inflation #2 not allowed " + m.engine_result ) );
                    	}
                    })
                    .submit();
            },

            function (wfCB) {
                self.what = "Check all balances #2 / fail check";

                assert(failed==false);

                wfCB();
            }
        ]

        async.waterfall( steps, function ( error ) {
        	if ( error ) {
        		console.log( "failure detail: %s", JSON.stringify( error ) );
        	}
            assert(!error, self.what);
            done();
        });
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
    var tx_fee = 12; //TODO: get tx fee

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

        inflation(1)
          .on('submitted', function (m) {
            if (m.engine_result === 'tesSUCCESS') {
            	$.remote.ledger_accept();    // Move it along.
				callback( null );
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

          	//console.log( "was %s, now %s", start_balance, m.node.Balance );
          	var diff = m.node.Balance - start_balance;
          	diff = diff - newCoinsFromInflation(6*tx_fee); // 4 (account create)+2 (set inflation dest) tx fees
          	//console.log( diff );

          	assert( diff >= -1 && diff <= 1 ); // rounding error could set things off a bit

            callback();
          }).request();
      },

      // extra transaction to cover tx history check
      function ( callback ) {
        self.what = "Distribute funds.";
        testutils.payment( $.remote, "alice", "bob", "5000.0", callback );
      },

      function(callback)
      {
        $.remote.ledger_accept();    // Move it along.
        callback(null);
      },

      // this is an extra check to verify that the tx history on carol is correct
      function ( callback ) {
        self.what = "check that tx stats are correct";
        var request = $.remote.requestAccountTx( { account: $.remote.account( 'carol' )._account_id }, function ( err, r ) {
            if ( err ) {
                callback( err );
            }
            //console.log( "account tx: %s", JSON.stringify( r, undefined, 2 ) );
            assert( r.transactions.length == 2 ); // account creation, inflation
            callback( null );
        } );
      }
    ]

    async.waterfall(steps,function (error) {
      assert(!error, self.what);
      done();
    });
  });

  test('Inflation #2 - 50/50 split', function(done) {
    var self = this;
    var start_balance = 0;
    var tx_fee = 12; //TODO: get tx fee

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
      function ( callback ) {
      	self.what = "Check starting balance";

      	$.remote.requestAccountBalance( $.remote.account( 'carol' )._account_id, 'current', null )
          .on( 'success', function ( m ) {
          	start_balance = m.node.Balance;
          	callback();
          } ).request();
      },
      function ( callback ) {
      	self.what = "Display ledger";

      	$.remote.request_ledger( 'current', true )
          .on( 'success', function ( m ) {
          	//console.log( "Ledger: %s", JSON.stringify( m, undefined, 2 ) );
          	callback();
          } ).on( 'error', function ( m ) {
          	console.log( "error: %s", JSON.stringify( m ) );
          	callback();
          } ).request();
      },

      function (callback) {
        self.what = "Do inflation";

        inflation(1)
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

            diff = diff - newCoinsFromInflation( 6 * tx_fee ) / 2; // 4 (account create)+2 (set inflation dest) tx fees

            assert( diff >= -1 && diff <= 1 ); // rounding error could set things off a bit

            callback();
          }).request();
      },


      function (callback) {
        self.what = "Check carol's balance";

        $.remote.requestAccountBalance($.remote.account('carol')._account_id, 'current', null)
          .on('success', function (m) {

          	//console.log( "was %s, now %s", start_balance, m.node.Balance );
          	var diff = m.node.Balance - start_balance;
          	diff = diff - newCoinsFromInflation( 6 * tx_fee )/2; // 4 (account create)+2 (set inflation dest) tx fees

            assert( diff >= -1 && diff <= 1 ); // rounding error could set things off a bit

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
