#include "cmd_registry.h"
#include <string.h>

struct CmdEntry
{
    const char *prefix;
    CmdHandler  handler;
    bool        exactMatch; // true when prefix has no trailing ':'
};

static CmdEntry s_entries[CMD_REGISTRY_MAX];
static uint8_t  s_count = 0;

void cmd_register(const char *prefix, CmdHandler handler)
{
    if (s_count >= CMD_REGISTRY_MAX)
    {
        Serial.println("[CMD] Registry full — increase CMD_REGISTRY_MAX");
        return;
    }
    size_t len = strlen(prefix);
    bool exact = (len == 0 || prefix[len - 1] != ':');
    s_entries[s_count++] = { prefix, handler, exact };
}

bool cmd_dispatch(const String &msg)
{
    for (uint8_t i = 0; i < s_count; i++)
    {
        const CmdEntry &e = s_entries[i];
        bool matched = e.exactMatch
                           ? (msg == e.prefix)
                           : msg.startsWith(e.prefix);
        if (matched)
        {
            e.handler(msg);
            return true;
        }
    }
    return false;
}
