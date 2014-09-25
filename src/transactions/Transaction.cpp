#include "Transaction.h"

namespace stellar
{
	TER Transaction::apply()
	{
		return(doApply());
	}
}
