#include "defines.h"
#include "connection.h"
#include "socketConnection.h"
#include "usbConnection.h"
#include "commandUtil.h"

using namespace Connection;
using namespace CommandUtil;

#define TITLE_ID 0x430000000000000B
#define HEAP_SIZE 0x00480000
#define THREAD_SIZE 0x1A000
#define VERSION_S "2.4.1"

ConnectionHandler* m_connection;

extern "C" {
    u32 __nx_applet_type = AppletType_None;

    void __libnx_initheap(void)
    {
        static u8 inner_heap[HEAP_SIZE];
        extern void* fake_heap_start;
        extern void* fake_heap_end;

        // Configure the newlib heap.
        fake_heap_start = inner_heap;
        fake_heap_end = inner_heap + sizeof(inner_heap);
    }
}

extern "C" {
    TimeServiceType __nx_time_service_type = TimeServiceType_System;

    void __appInit(void)
    {
        Result rc;
        svcSleepThread(20000000000L);

        rc = smInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);

        if (hosversionGet() == 0) {
            rc = setsysInitialize();
            if (R_SUCCEEDED(rc)) {
                SetSysFirmwareVersion fw;
                rc = setsysGetFirmwareVersion(&fw);
                if (R_SUCCEEDED(rc))
                    hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
                setsysExit();
            }
        }

        rc = timeInitialize();
        if (R_FAILED(rc))
        {
            timeExit();
            __nx_time_service_type = TimeServiceType_User;
            rc = timeInitialize();
            if (R_FAILED(rc))
                fatalThrow(rc);
        }

        rc = pmdmntInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);

        rc = ldrDmntInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);

        rc = pminfoInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);

        rc = fsInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);

        rc = fsdevMountSdmc();
        if (R_FAILED(rc))
            fatalThrow(rc);

        if (m_connection->usb())
            m_connection = new UsbConnection::UsbConnection{};
        else m_connection = new SocketConnection::SocketConnection{};

        if (!m_connection->initialize())
            fatalThrow(rc);

        rc = capsscInitialize();
        if (R_FAILED(rc))
            fatalThrow(rc);

        rc = viInitialize(ViServiceType_Default);
        if (R_FAILED(rc))
            fatalThrow(rc);
    }
}

extern "C" void __appExit(void)
{
    smExit();
    timeExit();
    pmdmntExit();
    ldrDmntExit();
    pminfoExit();
    m_connection->disconnect();
    delete(m_connection);
    capsscExit();
    viExit();
}

int main()
{
    m_connection->connect();
    CommandUtils* utils = new CommandUtils();
    utils->flashLed();
    delete(utils);
    return 0;
}
