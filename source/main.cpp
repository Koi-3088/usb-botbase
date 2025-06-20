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

ConnectionHandler* m_connection;

void setUpConnection() {
    if (Utils::isUSB()) {
        m_connection = new UsbConnection::UsbConnection();
    } else {
        m_connection = new SocketConnection::SocketConnection();
    }
}

extern "C" {
    u32 __nx_applet_type = AppletType_None;
    TimeServiceType __nx_time_service_type = TimeServiceType_System;

    void __libnx_initheap(void) {
        static u8 inner_heap[0x300000];
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

        rc = capsscInitialize();
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }

        rc = viInitialize(ViServiceType_Default);
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }

        setUpConnection();
        if (R_FAILED(m_connection->initialize(rc))) {
            fatalThrow(rc);
        }
    }

    void __appExit(void) {
        smExit();
        timeExit();
        pmdmntExit();
        ldrDmntExit();
        pminfoExit();

        if (m_connection) {
            m_connection->disconnect();
            delete m_connection;
            m_connection = nullptr;
        }

        capsscExit();
        viExit();
    }
}

int main() {
    try {
        Logger::logToFile("\n##########\r\n");
        Logger::logToFile("Starting main()...");

        while (appletMainLoop()) {
            Logger::logToFile("Connecting...");
            if (m_connection->connect()) {
                m_connection->run();
                m_connection->disconnect();
                delete m_connection;
                m_connection = nullptr;
            }

            Logger::logToFile("Resetting connection...");
            setUpConnection();
        }

        Logger::logToFile("Exiting main()...");
    } catch (const std::exception& e) {
        Logger::logToFile("Exception caught during initialization: " + std::string(e.what()));
        return -1;
    }

    return 0;
}
