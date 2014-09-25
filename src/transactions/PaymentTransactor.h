#ifndef __PAYMENTTRANSACTOR__
#define __PAYMENTTRANSACTOR__
#include "Transactor.h"

namespace stellar {

	class PaymentTransactorLog;

	

	class PaymentTransactor : public Transactor
	{
	public:
		PaymentTransactor(
			SerializedTransaction const& txn,
			TransactionEngineParams params,
			TransactionEngine* engine)
			: Transactor(
			txn,
			params,
			engine,
			LogPartition::getJournal <PaymentTransactorLog>())
		{

		}

		TER doApply();
	};

}

#endif
