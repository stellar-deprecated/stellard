var assert    = require('assert');
var Remote    = require("stellar-lib").Remote;
var testutils = require("./testutils.js");
var config    = testutils.init_config();

suite('Remote functions', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test('request_ledger_current', function(done) {
    $.remote.request_ledger_current(function(err, m) {
      assert(!err);
      assert.strictEqual(m.ledger_current_index, 3);
      done();
    });
  });

  test('request_ledger_hash', function(done) {
    $.remote.request_ledger_hash(function(err, m) {
      // console.log("result: %s", JSON.stringify(m));
      assert(!err);
      assert.strictEqual(m.ledger_index, 2);
      done();
    })
  });

  test('manual account_root success', function(done) {
    var self = this;

    $.remote.request_ledger_hash(function(err, r) {
      //console.log("result: %s", JSON.stringify(r));
      assert(!err);
      assert('ledger_hash' in r);

      var request = $.remote.request_ledger_entry('account_root')
      .ledger_hash(r.ledger_hash)
      .account_root("ganVp9o5emfzpwrG5QVUXqMv8AgLcdvySb");

      request.callback(function(err, r) {
        // console.log("account_root: %s", JSON.stringify(r));
        assert(!err);
        assert('node' in r);
        done();
      });
    });
  });

  test('account_root remote malformedAddress', function(done) {
    var self = this;

    $.remote.request_ledger_hash(function(err, r) {
      // console.log("result: %s", JSON.stringify(r));
      assert(!err);

      var request = $.remote.request_ledger_entry('account_root')
      .ledger_hash(r.ledger_hash)
      .account_root("zHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh");

      request.callback(function(err, r) {
        // console.log("account_root: %s", JSON.stringify(r));
        assert(err);
        assert.strictEqual(err.error, 'remoteError');
        assert.strictEqual(err.remote.error, 'malformedAddress');
        done();
      });
    })
  });

  test('account_root entryNotFound', function(done) {
    var self = this;

    $.remote.request_ledger_hash(function(err, r) {
      // console.log("result: %s", JSON.stringify(r));
      assert(!err);

      var request = $.remote.request_ledger_entry('account_root')
      .ledger_hash(r.ledger_hash)
      .account_root("alice");

      request.callback(function(err, r) {
        // console.log("error: %s", m);
        assert(err);
        assert.strictEqual(err.error, 'remoteError');
        assert.strictEqual(err.remote.error, 'entryNotFound');
        done();
      });
    })
  });



  test('create account', function(done) {
    var self = this;

    var root_id = $.remote.account('root')._account_id;

    $.remote.request_subscribe().accounts(root_id).request();

    $.remote.transaction()
    .payment('root', 'alice', "10000.0")
    .once('error', done)
    .once('proposed', function(res) {
      //console.log('Submitted', res);
      $.remote.ledger_accept();
    })
    .once('success', function (r) {
      //console.log("account_root: %s", JSON.stringify(r));
      // TODO: Need to verify account and balance.
      done();
    })
    .submit();
  });

});

//vim:sw=2:sts=2:ts=8:et
