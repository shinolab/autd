//
//  ethercat_link.cpp
//  autd3
//
//  Created by Seki Inoue on 6/1/16.
//
//

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/assert.hpp>
#include <AdsLib.h>
#include "privdef.hpp"
#include "ethercat_link.hpp"

// XXX: should be configuarable?
#define INDEX_GROUP (0x3040030)
#define INDEX_OFFSET_BASE (0x81000000)
#define PORT (301)

void autd::internal::EthercatLink::Open(std::string location) {
    std::vector<std::string> sep;
    boost::algorithm::split(sep, location, boost::is_any_of(":"));
    if (sep.size() == 1) {
        this->Open(sep[0], "");
    } else if (sep.size() == 2) {
        this->Open(sep[1], sep[0]);
    } else {
        BOOST_ASSERT_MSG(false, "Invalid address");
    }
}

void autd::internal::EthercatLink::Open(std::string ams_net_id, std::string ipv4addr) {
    std::vector<std::string> octets;
    boost::algorithm::split(octets, ams_net_id, boost::is_any_of("."));
    BOOST_ASSERT_MSG(octets.size() == 6, "Ams net id must have 6 octets");
    if (ipv4addr == "") {
        // Extract ipv6 addr from leading four octets of the ams net id.
        for (int i = 0; i < 3; i++) {
            ipv4addr += octets[i]+".";
        }
        ipv4addr += octets[3];
    }

    this->_netId = {
        static_cast<uint8_t>(std::stoi(octets[0])),
        static_cast<uint8_t>(std::stoi(octets[1])),
        static_cast<uint8_t>(std::stoi(octets[2])),
        static_cast<uint8_t>(std::stoi(octets[3])),
        static_cast<uint8_t>(std::stoi(octets[4])),
        static_cast<uint8_t>(std::stoi(octets[5]))
    };

    if (AdsAddRoute(this->_netId, ipv4addr.c_str())) {
        std::cerr << "Error: Could not connect to remote." << std::endl;
        return;
    }

    // open a new ADS port
    this->_port = AdsPortOpenEx();
    if (!this->_port) {
        std::cerr << "Error: Failed to open a new ADS port." << std::endl;
    }
}

void autd::internal::EthercatLink::Close() {
    AdsPortCloseEx(this->_port);
    this->_port = 0;
}

bool autd::internal::EthercatLink::isOpen() {
    return (this->_port > 0);
}

void autd::internal::EthercatLink::Send(size_t size, std::unique_ptr<uint8_t[]> buf) {
    const AmsAddr pAddr = {this->_netId, PORT};

    long ret = AdsSyncWriteReqEx(this->_port, // NOLINT
                                 &pAddr, INDEX_GROUP,
                                 INDEX_OFFSET_BASE,
                                 size,
                                 &buf[0]);

    if (ret > 0) {
        switch (ret) {
            case ADSERR_DEVICE_INVALIDSIZE:
                std::cerr << "The number of devices is invalid." << std::endl;
                break;
            default:
                std::cerr << "Error on sending data: " << std::hex << ret << std::endl;
        }

        throw static_cast<int>(ret);
    }
}

// for localhost connection
#ifdef _WIN32

typedef long(_stdcall *TcAdsPortOpenEx)(void); // NOLINT
typedef long(_stdcall *TcAdsPortCloseEx)(long); // NOLINT
typedef long(_stdcall *TcAdsGetLocalAddressEx)(long, AmsAddr*); // NOLINT
typedef long(_stdcall *TcAdsSyncWriteReqEx)(long, AmsAddr*, unsigned long, unsigned long, unsigned long, void*); // NOLINT

#ifdef _WIN64

#define TCADS_AdsPortOpenEx "AdsPortOpenEx"
#define TCADS_AdsGetLocalAddressEx "AdsGetLocalAddressEx"
#define TCADS_AdsPortCloseEx "AdsPortCloseEx"
#define TCADS_AdsSyncWriteReqEx "AdsSyncWriteReqEx"

#else

#define TCADS_AdsPortOpenEx "_AdsPortOpenEx@0"
#define TCADS_AdsGetLocalAddressEx "_AdsGetLocalAddressEx@8"
#define TCADS_AdsPortCloseEx "_AdsPortCloseEx@4"
#define TCADS_AdsSyncWriteReqEx "_AdsSyncWriteReqEx@24"

#endif

void autd::internal::LocalEthercatLink::Open(std::string location) {
	this->lib = LoadLibrary("TcAdsDll.dll");
//	this->lib = LoadPackagedLibrary(TEXT("TcAdsDll.dll"), 0);

    if (lib == NULL) {
        BOOST_ASSERT_MSG(false, "couldn't find TcADS-DLL.");
    }

    // open a new ADS port
    TcAdsPortOpenEx portOpen = (TcAdsPortOpenEx)GetProcAddress(this->lib, TCADS_AdsPortOpenEx);
    this->_port = (*portOpen)();
    if (!this->_port) {
        std::cerr << "Error: Failed to open a new ADS port." << std::endl;
    }

    AmsAddr addr;
    TcAdsGetLocalAddressEx getAddr = (TcAdsGetLocalAddressEx)GetProcAddress(this->lib, TCADS_AdsGetLocalAddressEx);
    long nErr = getAddr(this->_port, &addr); // NOLINT
    if (nErr) std::cerr << "Error: AdsGetLocalAddress: " << nErr << std::endl;
    this->_netId = addr.netId;
}

void autd::internal::LocalEthercatLink::Close() {
    TcAdsPortCloseEx portClose = (TcAdsPortCloseEx)GetProcAddress(this->lib, TCADS_AdsPortCloseEx);
    (*portClose)(this->_port);
    this->_port = 0;
}

void autd::internal::LocalEthercatLink::Send(size_t size, std::unique_ptr<uint8_t[]> buf) {
    AmsAddr addr = {this->_netId, PORT};

    TcAdsSyncWriteReqEx write = (TcAdsSyncWriteReqEx)GetProcAddress(this->lib, TCADS_AdsSyncWriteReqEx);
    long ret = write(this->_port, // NOLINT
        &addr, INDEX_GROUP,
        INDEX_OFFSET_BASE,
        size,
        &buf[0]);

    if (ret > 0) {
        // https://infosys.beckhoff.com/english.php?content=../content/1033/tcadscommon/html/tcadscommon_intro.htm&id=
        // 6 : target port not found
        std::cerr << "Error on sending data (local): " << std::hex << ret << std::endl;
        throw (int)ret;
    }
}


#else

void autd::internal::LocalEthercatLink::Open(std::string location) {
    BOOST_ASSERT_MSG(false, "Link to localhost has not been compiled. Rebuild this library on a Twincat3 host machine with TcADS-DLL.");
}

void autd::internal::LocalEthercatLink::Close() {

}

void autd::internal::LocalEthercatLink::Send(size_t size, std::unique_ptr<uint8_t[]> buf) {

}

#endif // TC_ADS
