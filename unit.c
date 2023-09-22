
#include "unit.h"

#include <dirent.h>

int unit_find(const uint16_t id, const char *const p_path, char *p_name)
{
    glob_t result;
    char globpath[255] = {0};

    ///@todo evaluate return code
    (void)snprintf(globpath, sizeof(globpath), "%s/%08X-*.cart", p_path, id);
    const int rc = glob(globpath, 0, NULL, &result);
    if (rc == 0)
    {
        for (size_t i = 0; i < result.gl_pathc; i++)
        {
            puts(result.gl_pathv[i]);
        }
    }
    else
    {
        // error
    }
    globfree(&result);
}

unit_parse_result_t unit_parse(unit_t **pp_unit, const char *const p_path)
{
    return UNIT_PARSE_OKAY;
}
