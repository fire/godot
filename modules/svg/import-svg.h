
#pragma once

#include "modules/svg/msdfgen/core/Shape.h"

#include "nanosvg.h"

namespace msdfgen {

/// Reads the first path found in the specified SVG file and stores it as a Shape in output.
bool loadNanoSvgShape(msdfgen::Shape &output, const NSVGshape* nsvgImage);
}
