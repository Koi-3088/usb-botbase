#include "defines.h"
#include "socketConnection.h"
#include "usbConnection.h"
#include "util.h"
#include "connection.h"
#include <switch.h>
#include "logger.h"

using namespace Connection;
using namespace Util;
using namespace SbbLog;

#define TITLE_ID 0x430000000000000B
#define HEAP_SIZE 0x300000
#define THREAD_SIZE 0x1A000
#define VERSION_S "2.4.1"

ConnectionHandler* m_connection;

extern "C" {
    u32 __nx_applet_type = AppletType_None;
    TimeServiceType __nx_time_service_type = TimeServiceType_System;

    void __libnx_initheap(void) {
        static u8 inner_heap[HEAP_SIZE];
        extern void* fake_heap_start;
        extern void* fake_heap_end;

        // Configure the newlib heap.
        fake_heap_start = inner_heap;
        fake_heap_end = inner_heap + sizeof(inner_heap);
    }

    void __appInit(void) {
        svcSleepThread(20000000000L);

        Result rc = smInitialize();
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }

        if (hosversionGet() == 0) {
            rc = setsysInitialize();
            if (R_SUCCEEDED(rc)) {
                SetSysFirmwareVersion fw;
                rc = setsysGetFirmwareVersion(&fw);
                if (R_SUCCEEDED(rc)) {
                    hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
                }

                setsysExit();
            }
        }

        rc = timeInitialize();
        if (R_FAILED(rc)) {
            timeExit();
            __nx_time_service_type = TimeServiceType_User;
            rc = timeInitialize();
            if (R_FAILED(rc)) {
                fatalThrow(rc);
            }
        }

        rc = pmdmntInitialize();
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }

        rc = ldrDmntInitialize();
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }

        rc = pminfoInitialize();
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }

        rc = fsInitialize();
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }

        rc = fsdevMountSdmc();
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }

        if (Utils::isUSB()) {
            m_connection = new UsbConnection::UsbConnection();
        }
        else {
            m_connection = new SocketConnection::SocketConnection();
        }

        if (R_FAILED(m_connection->initialize(rc))) {
            fatalThrow(rc);
        }

        rc = capsscInitialize();
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }

        rc = viInitialize(ViServiceType_Default);
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }
    }

    void __appExit(void) {
        smExit();
        timeExit();
        pmdmntExit();
        ldrDmntExit();
        pminfoExit();
        m_connection->disconnect();
        delete m_connection;
        m_connection = nullptr;
        capsscExit();
        viExit();
    }
}

int main() {
    Logger::logToFile("\n##########\r\n");
    m_connection->connect();
    delete m_connection;
    m_connection = nullptr;
    Logger::logToFile("Exiting main()...");
    return 0;
}
