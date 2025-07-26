#include "defines.h"
#include "socketConnection.h"
#include "usbConnection.h"
#include "util.h"
#include "connection.h"
#include <switch.h>
#include "logger.h"
#include <memory>

using namespace Connection;
using namespace Util;
using namespace SbbLog;

#define TITLE_ID 0x430000000000000B
std::unique_ptr<ConnectionHandler> m_connection;

extern "C" {
    u32 __nx_applet_type = AppletType_None;
    TimeServiceType __nx_time_service_type = TimeServiceType_System;

    void setUpConnection() {
        try {
            if (Utils::isUSB()) {
                m_connection = std::make_unique<UsbConnection::UsbConnection>();
            } else {
                m_connection = std::make_unique<SocketConnection::SocketConnection>();
            }
        } catch (const std::exception& e) {
            Logger::instance().log("Exception caught while setting up connection.", e.what());
        }
    }

    void __libnx_initheap(void) {
        static u8 inner_heap[0x300000];
        extern void* fake_heap_start;
        extern void* fake_heap_end;

        // Configure the newlib heap.
        fake_heap_start = inner_heap;
        fake_heap_end = inner_heap + sizeof(inner_heap);
    }

    void __appInit(void) {
        svcSleepThread(5e+9L);

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
            m_connection.reset();

            if (Utils::isUSB()) {
                usbCommsExit();
            } else {
                socketExit();
            }
        }

        capsscExit();
        viExit();
    }

    int main(int argc, char** argv) {
        Logger::instance().log("##########\r\n##########\r\nStarting main()...", "", true);
        while (true) {
            try {
                Logger::instance().log("Connecting...", "", true);
                if (m_connection->connect()) {
                    m_connection->run();
                    m_connection->disconnect();
                }

                Logger::instance().log("Resetting connection...", "", true);
                m_connection.reset();
                setUpConnection();
            } catch (...) {
                Result rc = Result { 0x1001 };
                Logger::instance().log("Exception caught in main().", std::to_string(R_DESCRIPTION(rc)));
                fatalThrow(rc);
            }
        }

        Logger::instance().log("Exiting main()...", "", true);
        return 0;
    }
}
