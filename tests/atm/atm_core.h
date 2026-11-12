/* Host-test include shim: atm_core.c does `#include "atm/atm_core.h"`,
 * which the 3DS build resolves via -I$(ATM_CORE_DIR)/include (vendored
 * shared ATM firmware). Host tests have no ATM_CORE_DIR, so redirect to
 * the in-tree copy of the same header. Tests/ is outside gate scope. */
#include <atm_core.h>
