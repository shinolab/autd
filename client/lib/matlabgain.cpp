//
//  matlabgain.cpp
//  autd3
//
//  Created by Seki Inoue on 9/20/16.
//
//


#include <stdio.h>
#include <complex>
#include "autd3.hpp"
#include "privdef.hpp"

#ifdef MATLAB_ENABLED
#include "mat.h"
#endif


template <typename T>
T clamp(T val, T _min, T _max) {
    return std::max<T>(std::min<T>(val, _max), _min);
}

autd::GainPtr autd::MatlabGain::Create(std::string filename, std::string varname) {
    #ifndef MATLAB_ENABLED
        throw new std::runtime_error("MatlabGain requires Matlab libraries. Recompile with Matlab Environment.");
    #endif    
    std::shared_ptr<MatlabGain> ptr = std::shared_ptr<MatlabGain>(new MatlabGain());
    ptr->_filename = filename;
    ptr->_varname = varname;
    return ptr;
}

void autd::MatlabGain::build() {
    if (this->built()) return;
    if (this->geometry() == nullptr) throw new std::runtime_error("Geometry is required to build Gain");
    
#ifdef MATLAB_ENABLED
    this->_data.clear();
    const int ntrans = this->geometry()->numTransducers();
    
    MATFile *pmat = matOpen(_filename.c_str(), "r");
    if (pmat == NULL) {
        throw new std::runtime_error("Cannot open a file "+_filename);
    }
    
    mxArray *arr = matGetVariable(pmat, _varname.c_str());
    size_t nelems = mxGetNumberOfElements(arr);
    if (ntrans < nelems) {
        throw new std::runtime_error("Insufficient number of data in mat file");
    }
    
    double *real = mxGetPr(arr);
    double *imag = mxGetPi(arr);
    
    this->_data.clear();
    const int ndevice = this->geometry()->numDevices();
    for (int i = 0; i < ndevice; i++) {
        this->_data[this->geometry()->deviceIdForDeviceIdx(i)].resize(NUM_TRANS_IN_UNIT);
    }
    for (int i = 0; i < nelems; i++) {
        float famp = sqrtf(real[i]*real[i] + imag[i]*imag[i]);
        uint8_t amp = clamp<float>(famp, 0, 1) * 255.99;
        float fphase = 0.0f;
        if (amp != 0) fphase = atan2f(imag[i], real[i]);
        uint8_t phase = round((-fphase + M_PI) / (2.0 * M_PI) * 255.0);
        
        this->_data[this->geometry()->deviceIdForTransIdx(i)][i%NUM_TRANS_IN_UNIT] = ((uint16_t)amp << 8) + phase;
    }
#endif
    
    this->_built = true;
}


