/* The shared text layer is reachable from the host tests. */
#include <assert.h>
#include "zk_text.h"

int main(void)
{
    assert(zt_valid("\xc5\xbc", 2));
    assert(!zt_valid("\xff", 1));
    return 0;
}
