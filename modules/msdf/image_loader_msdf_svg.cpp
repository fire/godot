#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/engine.h"
#include "core/image.h"
#include "core/io/resource_importer.h"
#include "core/io/resource_saver.h"
#include "core/os/file_access.h"

#include "thirdparty/msdfgen/ext/import-svg.h"
#include "thirdparty/msdfgen/msdfgen.h"

#define LARGE_VALUE 1e240
#define SDF_ERROR_ESTIMATE_PRECISION 19

#include "image_loader_msdf_svg.h"