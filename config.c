#include "config.h"

#include <errno.h>
#include <mini.h>
#include <string.h>

#include "log.h"

int config_load(const char *const p_filename, config_t *p_config)
{
    int ret = 0;
    mini_t *p_mini = mini_init(p_filename);
    if (!p_mini)
        return EINVAL;
    while (mini_next(p_mini))
    {
        if (!p_mini->key)
        {
            // ignore sections
        }
        else if (!p_mini->value)
        {
            // ignore unkeyed values
        }
        else
        {
            if (strncmp(p_mini->key, "db_path", strlen("db_path") - 1) == 0)
            {
                strcpy(p_config->cartdb_path, p_mini->value);
            }
            else if (strncmp(p_mini->key, "notifications", strlen("notifications") - 1) == 0)
            {
                if (strncmp(p_mini->value, "yes", strlen("yes") - 1) == 0)
                    p_config->notification_enabled = true;
                else
                    p_config->notification_enabled = false;
            }
        }
    }
    if (!p_mini->eof)
    {

        LOG_ERR("Error reading configuration at '%s' (error '%s')", p_filename, strerror(errno));
        ret = 1;
    }
    mini_free(p_mini);
    return ret;
}
