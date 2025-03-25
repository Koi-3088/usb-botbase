#include <string.h>
#include "defines.h"
#include "connection.h"
#include "socketConnection.h"
#include "usbConnection.h"

namespace Connection {
    bool ConnectionHandler::isUSB()
    {
        char str[4];
        FILE* config = fopen("sdmc:/atmosphere/contents/430000000000000B/config.cfg", "r");
        if (config)
        {
            fscanf(config, "%[^\n]", str);
            fclose(config);
        }

        fsExit();
        fsdevUnmountAll();
        return strcmp(strlwr(str), "wifi") != 0;
    }
}
