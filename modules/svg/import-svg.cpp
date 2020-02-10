#include "import-svg.h"
#include "nanosvg.h"

namespace msdfgen {

bool loadNanoSvgShape(Shape &output, const NSVGshape *shape) {
    output.inverseYAxis = true;
    for (NSVGpath *path = shape->paths; path != NULL; path = path->next) {      
        Contour &contour = output.addContour();
        for (int i = 0; i < path->npts-1; i += 3) {
            float* p = &path->pts[i*2];
            EdgeHolder edge = new CubicSegment(Point2(p[0], p[1]), Point2(p[2], p[3]), Point2(p[4],p[5]), Point2(p[6],p[7]), (EdgeColor) shape->fill.color);
            contour.addEdge(edge);
        }
    }
    return true;
}
}
