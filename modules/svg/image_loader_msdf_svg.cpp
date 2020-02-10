#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/engine.h"
#include "core/image.h"

#include "msdfgen.h"

#include "import-svg.h"
#include "nanosvg.h"

#define LARGE_VALUE 1e240
#define SDF_ERROR_ESTIMATE_PRECISION 19

enum Format {
	AUTO,
	PNG
};

static bool is8bitFormat(Format format) {
	return format == PNG;
}

static char toupper(char c) {
	return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

static bool writeTextBitmap(FILE *file, const float *values, int cols, int rows) {
	for (int row = 0; row < rows; ++row) {
		for (int col = 0; col < cols; ++col) {
			int v = msdfgen::clamp(int((*values++) * 0x100), 0xff);
			fprintf(file, col ? " %02X" : "%02X", v);
		}
		fprintf(file, "\n");
	}
	return true;
}

template <int N>
static void invertColor(const msdfgen::BitmapRef<float, N> &bitmap) {
	const float *end = bitmap.pixels + N * bitmap.width * bitmap.height;
	for (float *p = bitmap.pixels; p < end; ++p)
		*p = 1.f - *p;
}

static bool cmpExtension(const char *path, const char *ext) {
	for (const char *a = path + strlen(path) - 1, *b = ext + strlen(ext) - 1; b >= ext; --a, --b)
		if (a < path || toupper(*a) != toupper(*b))
			return false;
	return true;
}

template <int N>
static const char *writeOutput(const msdfgen::BitmapConstRef<float, N> &bitmap, const char *filename, Format format) {
	if (filename) {
		if (format == AUTO) {
			if (cmpExtension(filename, ".png"))
				format = PNG;
			else
				return "Could not deduce format from output file name.";
		}
		switch (format) {
			// case PNG: return msdfgen::savePng(bitmap, filename) ? NULL : "Failed to write output PNG image.";
			default:;
		}
	} else {
		if (format == AUTO)
			writeTextBitmap(stdout, bitmap.pixels, N * bitmap.width, bitmap.height);
		else
			return "Unsupported format for standard output.";
	}
	return NULL;
}

Error msdf_output(char *p_input, Ref< ::Image> &r_image) {
	enum {
		SINGLE,
		PSEUDO,
		MULTI,
		METRICS
	} mode = MULTI;
	bool overlapSupport = true;
	bool scanlinePass = true;
	msdfgen::FillRule fillRule = msdfgen::FILL_NONZERO;
	Format format = AUTO;
	const char *output = "output.png";
	const char *testRender = NULL;
	const char *testRenderMulti = NULL;
	bool outputSpecified = false;
	int svgPathIndex = 0;

	int width = 64, height = 64;
	int testWidthM = 0, testHeightM = 0;
	bool autoFrame = false;
	enum {
		RANGE_UNIT,
		RANGE_PX
	} rangeMode = RANGE_PX;
	double range = 1;
	double pxRange = 2;
	msdfgen::Vector2 translate;
	msdfgen::Vector2 scale = 1;
	bool scaleSpecified = false;
	double angleThreshold = 3;
	double edgeThreshold = 1.001;
	bool yFlip = false;
	bool printMetrics = false;
	bool estimateError = false;
	bool skipColoring = false;
	enum {
		KEEP,
		REVERSE,
		GUESS
	} orientation = KEEP;
	unsigned long long coloringSeed = 0;

	// Custom settings
	{
		autoFrame = true;
		yFlip = true;
		printMetrics = true;
		estimateError = true;
	}

	msdfgen::Vector2 svgDims;
	double glyphAdvance = 0;
	msdfgen::Shape shape;
	struct NSVGimage *image = nsvgParse(p_input, "px", 96);
	svgDims = msdfgen::Vector2(image->width, image->height);
	for (NSVGshape *nsvgShape = image->shapes; nsvgShape != NULL; nsvgShape = nsvgShape->next) {
		msdfgen::Shape current_shape;
		if (!loadNanoSvgShape(current_shape, nsvgShape)) {
			nsvgDelete(image);
			ERR_FAIL_V_MSG(FAILED, "Failed to load shape from SVG file.");
		}
		shape = current_shape;
		svgPathIndex++;
	}
	nsvgDelete(image);

	// Validate and normalize shape
	if (!shape.validate())
		ERR_FAIL_V_MSG(FAILED, "The geometry of the loaded shape is invalid.");
	shape.normalize();
	if (yFlip)
		shape.inverseYAxis = !shape.inverseYAxis;

	double avgScale = .5 * (scale.x + scale.y);
	struct {
		double l, b, r, t;
	} bounds = {
		LARGE_VALUE, LARGE_VALUE, -LARGE_VALUE, -LARGE_VALUE
	};
	if (autoFrame || mode == METRICS || printMetrics || orientation == GUESS)
		shape.bounds(bounds.l, bounds.b, bounds.r, bounds.t);

	// Auto-frame
	if (autoFrame) {
		double l = bounds.l, b = bounds.b, r = bounds.r, t = bounds.t;
		msdfgen::Vector2 frame(width, height);
		if (rangeMode == RANGE_UNIT)
			l -= .5 * range, b -= .5 * range, r += .5 * range, t += .5 * range;
		else if (!scaleSpecified)
			frame -= pxRange;
		if (l >= r || b >= t)
			l = 0, b = 0, r = 1, t = 1;
		if (frame.x <= 0 || frame.y <= 0)
			ERR_FAIL_V_MSG(FAILED, "Cannot fit the specified pixel range.");
		msdfgen::Vector2 dims(r - l, t - b);
		if (scaleSpecified)
			translate = .5 * (frame / scale - dims) - msdfgen::Vector2(l, b);
		else {
			if (dims.x * frame.y < dims.y * frame.x) {
				translate.set(.5 * (frame.x / frame.y * dims.y - dims.x) - l, -b);
				scale = avgScale = frame.y / dims.y;
			} else {
				translate.set(-l, .5 * (frame.y / frame.x * dims.x - dims.y) - b);
				scale = avgScale = frame.x / dims.x;
			}
		}
		if (rangeMode == RANGE_PX && !scaleSpecified)
			translate += .5 * pxRange / scale;
	}

	if (rangeMode == RANGE_PX)
		range = pxRange / MIN(scale.x, scale.y);

	// Print metrics
	if (mode == METRICS || printMetrics) {
		FILE *out = stdout;
		if (mode == METRICS && outputSpecified)
			out = fopen(output, "w");
		if (!out)
			ERR_FAIL_V_MSG(FAILED, "Failed to write output file.");
		if (shape.inverseYAxis)
			fprintf(out, "inverseY = true\n");
		if (bounds.r >= bounds.l && bounds.t >= bounds.b)
			fprintf(out, "bounds = %.12g, %.12g, %.12g, %.12g\n", bounds.l, bounds.b, bounds.r, bounds.t);
		if (svgDims.x != 0 && svgDims.y != 0)
			fprintf(out, "dimensions = %.12g, %.12g\n", svgDims.x, svgDims.y);
		if (glyphAdvance != 0)
			fprintf(out, "advance = %.12g\n", glyphAdvance);
		if (autoFrame) {
			if (!scaleSpecified)
				fprintf(out, "scale = %.12g\n", avgScale);
			fprintf(out, "translate = %.12g, %.12g\n", translate.x, translate.y);
		}
		if (rangeMode == RANGE_PX)
			fprintf(out, "range = %.12g\n", range);
		if (mode == METRICS && outputSpecified)
			fclose(out);
	}

	// Compute output
	msdfgen::Bitmap<float, 3> msdf;

	if (!skipColoring)
		msdfgen::edgeColoringSimple(shape, angleThreshold, coloringSeed);
	msdf = msdfgen::Bitmap<float, 3>(width, height);
	msdfgen::generateMSDF(msdf, shape, range, scale, translate, scanlinePass ? 0 : edgeThreshold, overlapSupport);

	if (orientation == GUESS) {
		// Get sign of signed distance outside bounds
		msdfgen::Point2 p(bounds.l - (bounds.r - bounds.l) - 1, bounds.b - (bounds.t - bounds.b) - 1);
		double dummy;
		msdfgen::SignedDistance minDistance;
		for (std::vector<msdfgen::Contour>::const_iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour)
			for (std::vector<msdfgen::EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge) {
				msdfgen::SignedDistance distance = (*edge)->signedDistance(p, dummy);
				if (distance < minDistance)
					minDistance = distance;
			}
		orientation = minDistance.distance <= 0 ? KEEP : REVERSE;
	}
	if (orientation == REVERSE) {
		switch (mode) {
			case MULTI:
				invertColor<3>(msdf);
				break;
			default:;
		}
	}
	if (scanlinePass) {
		switch (mode) {
			case MULTI:
				msdfgen::distanceSignCorrection(msdf, shape, scale, translate, fillRule);
				if (edgeThreshold > 0)
					msdfgen::msdfErrorCorrection(msdf, edgeThreshold / (scale * range));
				break;
			default:;
		}
	}

	const char *error = NULL;

	error = writeOutput<3>(msdf, output, format);
	if (error)
		ERR_FAIL_V_MSG(FAILED, error);
	if (is8bitFormat(format) && (testRenderMulti || testRender || estimateError))
		msdfgen::simulate8bit(msdf);
	if (estimateError) {
		double sdfError = estimateSDFError(msdf, shape, scale, translate, SDF_ERROR_ESTIMATE_PRECISION, fillRule);
		printf("SDF error ~ %e\n", sdfError);
	}
	return OK;
}
