#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/engine.h"
#include "core/image.h"
#include "core/io/resource_importer.h"
#include "core/io/resource_saver.h"
#include "core/os/file_access.h"
#include "scene/resources/texture.h"

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

template <int N>
static void invertColor(const msdfgen::BitmapRef<float, N> &bitmap) {
	const float *end = bitmap.pixels + N * bitmap.width * bitmap.height;
	for (float *p = bitmap.pixels; p < end; ++p)
		*p = 1.f - *p;
}

Error msdf_output(char *p_input, Ref< ::Image> &r_image, Vector2 p_scale) {
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
	bool autoFrame = false;
	enum {
		RANGE_UNIT,
		RANGE_PX
	} rangeMode = RANGE_PX;
	double range = 1;
	double pxRange = 2;
	msdfgen::Vector2 translate;
	msdfgen::Vector2 scale = 1;
	scale.x = scale.x * p_scale.x;
	scale.y = scale.y * p_scale.y;
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
	if (autoFrame || printMetrics || orientation == GUESS)
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
	if (printMetrics) {
		FILE *out = stdout;
		if (outputSpecified)
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
		if (outputSpecified)
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
		invertColor<3>(msdf);
	}
	if (scanlinePass) {
		msdfgen::distanceSignCorrection(msdf, shape, scale, translate, fillRule);
		if (edgeThreshold > 0) {
			msdfgen::msdfErrorCorrection(msdf, edgeThreshold / (scale * range));
		}
	}

    const msdfgen::BitmapConstRef<float, 3> pixel = msdf;
	r_image->create(width, height, false, Image::Format::FORMAT_RGBA8);
	r_image->resize(width, height);
	r_image->lock();
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			Color c;
            const float* split_pixel = pixel(j,i);
            c.r = split_pixel[0];
            c.g = split_pixel[1];
            c.b = split_pixel[2];
			r_image->set_pixel(j, i, c);
		}
	}
    r_image->unlock();

	if (is8bitFormat(format) && (testRenderMulti || testRender || estimateError)) {
		msdfgen::simulate8bit(msdf);
    }
	if (estimateError) {
		double sdfError = estimateSDFError(msdf, shape, scale, translate, SDF_ERROR_ESTIMATE_PRECISION, fillRule);
		printf("SDF error ~ %e\n", sdfError);
	}
	return OK;
}

class ResourceImporterMSDF : public ResourceImporter {
	GDCLASS(ResourceImporterMSDF, ResourceImporter);
	Vector2 scale = Vector2(1.0f, 1.0f);
public:
	Vector2 get_scale() const {
		return scale;
	}
	void set_scale(const Vector2 p_scale) {
		scale = p_scale;
	}
	virtual String get_importer_name() const;
	virtual String get_visible_name() const;
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual String get_save_extension() const;
	virtual String get_resource_type() const;

	virtual int get_preset_count() const;
	virtual String get_preset_name(int p_idx) const;

	virtual void get_import_options(List<ImportOption> *r_options, int p_preset = 0) const;
	virtual bool get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const;
	virtual Error import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files = NULL, Variant *r_metadata = NULL);

	ResourceImporterMSDF();
	~ResourceImporterMSDF();
};

String ResourceImporterMSDF::get_importer_name() const {

	return "msdf";
}

String ResourceImporterMSDF::get_visible_name() const {

	return "MSDF";
}

void ResourceImporterMSDF::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("svg");
}

String ResourceImporterMSDF::get_save_extension() const {
	return "res";
}

String ResourceImporterMSDF::get_resource_type() const {

	return "Image";
}

bool ResourceImporterMSDF::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {

	return true;
}

int ResourceImporterMSDF::get_preset_count() const {
	return 0;
}
String ResourceImporterMSDF::get_preset_name(int p_idx) const {

	return String();
}

void ResourceImporterMSDF::get_import_options(List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::VECTOR2, "scale", PROPERTY_HINT_NONE), Vector2(1.0f, 1.0f)));
}

Error ResourceImporterMSDF::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	Vector2 scale = p_options["scale"];

	Error err;
	FileAccess *f = FileAccess::open(p_source_file, FileAccess::READ, &err);
	if (!f) {
		ERR_PRINTS("Error opening file '" + p_source_file + "'.");
		return err;
	}

	uint32_t size = f->get_len();
	PoolVector<uint8_t> src_image;
	src_image.resize(size + 1);
	PoolVector<uint8_t>::Write src_w = src_image.write();
	f->get_buffer(src_w.ptr(), size);
	src_w.ptr()[size] = '\0';

	Ref<Image> image;
	image.instance();
	msdf_output((char *)src_w.ptr(), image, scale);
	Ref<ImageTexture> tex;
	tex.instance();
	tex->create_from_image(image);
	return ResourceSaver::save(p_save_path + ".res", tex);
}

ResourceImporterMSDF::ResourceImporterMSDF() {
}

ResourceImporterMSDF::~ResourceImporterMSDF() {
}
