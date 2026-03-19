/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "vi.h"
#include <libdragon.h>

float P64::VI::calcRefreshRate()
{
#ifdef PLATFORM_PC
    return 60.0f;  /* N64 VI not available on PC; use common default */
#else
    return vi_get_refresh_rate();
#endif
}
