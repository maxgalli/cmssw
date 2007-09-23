//
// Base class for pixel calibration data
//

#include <string>
#include <iostream>
#include "CalibFormats/SiPixelObjects/interface/PixelCalibBase.h"

using namespace std;

PixelCalibBase::PixelCalibBase() {
  mode_ = "Default";
}

PixelCalibBase::~PixelCalibBase() {
}

std::string PixelCalibBase::mode() {
  return mode_;
}


