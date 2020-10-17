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
#include "editor/editor_file_system.h"
#include "scene/resources/texture.h"

#include "thirdparty/msdfgen/msdfgen.h"

#include "thirdparty/msdfgen/ext/import-svg.h"
#include "nanosvg.h"

#define LARGE_VALUE 1e240
#define SDF_ERROR_ESTIMATE_PRECISION 19

class ResourceImporterSVGDistanceField : public ResourceImporter {
	GDCLASS(ResourceImporterSVGDistanceField, ResourceImporter);
	Vector2 scale = Vector2(1.0f, 1.0f);
	bool auto_frame = false;

	enum Format {
		AUTO,
		PNG
	};

	template <int N>
	static void invertColor(const msdfgen::BitmapRef<float, N> &bitmap) {
		const float *end = bitmap.pixels + N * bitmap.width * bitmap.height;
		for (float *p = bitmap.pixels; p < end; ++p)
			*p = 1.f - *p;
	}

	struct MSDFState {
		Vector2 size = Vector2(1.0f, 1.0f);
		Vector2 scale = Vector2(1.0f, 1.0f);
		Vector2 translate;
		real_t range = 1.0;
		bool auto_frame = false;
	};

	Error msdf_output(char *p_file_name, Ref< ::Image> &r_image, MSDFState &p_state) {
		bool overlapSupport = true;
		bool scanlinePass = true;
		msdfgen::FillRule fillRule = msdfgen::FILL_NONZERO;
		int svgPathIndex = 0;

	int width = 256, height = 256;
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
		double angleThreshold = Math_PI;
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
			width = p_state.size.x;
			height = p_state.size.y;
			orientation = GUESS;

			scale.x = p_state.scale.y;
			scale.y = p_state.scale.x;
			scaleSpecified = !p_state.auto_frame;
			range = p_state.range;
			rangeMode = RANGE_UNIT;
			translate.x = p_state.translate.x;
			translate.y = p_state.translate.y;
			autoFrame = p_state.auto_frame;
			yFlip = true;
			printMetrics = true;
			estimateError = true;
		}

		msdfgen::Vector2 svgDims;
		double glyphAdvance = 0;
		msdfgen::Shape shape;
		if (!msdfgen::loadSvgShape(shape, p_file_name, svgPathIndex, &svgDims)) {
			ERR_FAIL_V_MSG(FAILED, "Failed to load shape from SVG file.");
		}

		// Validate and normalize shape
		if (!shape.validate()) {
			ERR_FAIL_V_MSG(FAILED, "The geometry of the loaded shape is invalid.");
		}
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
			shape.bound(bounds.l, bounds.b, bounds.r, bounds.t);

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
			if (shape.inverseYAxis)
				print_line("ResourceImporterMSDF inverseY is true");
			if (bounds.r >= bounds.l && bounds.t >= bounds.b)
				print_line("ResourceImporterMSDF bounds are " + rtos(bounds.l) + ", " + rtos(bounds.b) + ", " + rtos(bounds.r) + ", " + rtos(bounds.t));
			if (svgDims.x != 0 && svgDims.y != 0)
				print_line("ResourceImporterMSDF dimensions are " + rtos(svgDims.x) + ", " + rtos(svgDims.y));
			if (glyphAdvance != 0)
				print_line("ResourceImporterMSDF advance is " + rtos(glyphAdvance));
			if (autoFrame) {
				if (!scaleSpecified)
					print_line("ResourceImporterMSDF scale is " + rtos(avgScale));
				print_line("ResourceImporterMSDF translate is " + rtos(translate.x) + ", " + rtos(translate.y));
			}
			if (rangeMode == RANGE_PX)
				print_line("ResourceImporterMSDF range is " + rtos(range));
		}

		msdfgen::Bitmap<float, 4> msdf;

		if (!skipColoring)
			msdfgen::edgeColoringSimple(shape, angleThreshold, coloringSeed);
		msdf = msdfgen::Bitmap<float, 4>(width, height);

		msdfgen::generateMTSDF(msdf, shape, range, scale, translate, scanlinePass ? 0 : edgeThreshold, overlapSupport);

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
			invertColor<4>(msdf);
		}
		if (scanlinePass) {
			msdfgen::distanceSignCorrection(msdf, shape, scale, translate, fillRule);
			if (edgeThreshold > 0) {
				msdfgen::msdfErrorCorrection(msdf, edgeThreshold / (scale * range));
			}
		}
		r_image->create(width, height, false, Image::Format::FORMAT_RGBA8);
		r_image->resize(width, height);
		r_image->lock();
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				Color c;
				const float *split_pixel = msdf(j, i);
				c.r = split_pixel[0];
				c.g = split_pixel[1];
				c.b = split_pixel[2];
				c.a = split_pixel[3];
				r_image->set_pixel(j, i, c);
			}
		}
		r_image->unlock();

		if (estimateError) {
			double sdfError = estimateSDFError(msdf, shape, scale, translate, SDF_ERROR_ESTIMATE_PRECISION, fillRule);
			printf("SDF error ~ %e\n", sdfError);
		}
		return OK;
	}

public:
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

	ResourceImporterSVGDistanceField();
	~ResourceImporterSVGDistanceField();
};

String ResourceImporterSVGDistanceField::get_importer_name() const {

	return "distancefield";
}

String ResourceImporterSVGDistanceField::get_visible_name() const {

	return "TextureDistanceField";
}

void ResourceImporterSVGDistanceField::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("svg");
	p_extensions->push_back("svgz");
}

String ResourceImporterSVGDistanceField::get_save_extension() const {
	return "res";
}

String ResourceImporterSVGDistanceField::get_resource_type() const {

	return "ImageTexture";
}

bool ResourceImporterSVGDistanceField::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {

	return true;
}

int ResourceImporterSVGDistanceField::get_preset_count() const {
	return 0;
}
String ResourceImporterSVGDistanceField::get_preset_name(int p_idx) const {

	return String();
}

void ResourceImporterSVGDistanceField::get_import_options(List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::VECTOR2, "size", PROPERTY_HINT_RANGE, "1,512,64"), Vector2(128.0f, 128.0f)));
	r_options->push_back(ImportOption(PropertyInfo(Variant::VECTOR2, "scale", PROPERTY_HINT_RANGE, "0.01,32.0,0.01"), Vector2(1.0f, 1.0f)));
	r_options->push_back(ImportOption(PropertyInfo(Variant::VECTOR2, "translate", PROPERTY_HINT_RANGE, "0,65536,0.01"), Vector2(0.0f, 0.0f)));
	r_options->push_back(ImportOption(PropertyInfo(Variant::REAL, "range", PROPERTY_HINT_RANGE, "0,2,0.01"), 1.0f));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "auto_frame", PROPERTY_HINT_NONE), true));
}

Error ResourceImporterSVGDistanceField::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	MSDFState state;
	state.size = p_options["size"];
	state.scale = p_options["scale"];
	state.translate = p_options["translate"];
	state.range = p_options["range"];
	state.auto_frame = p_options["auto_frame"];
	Error err;
	FileAccess *f = FileAccess::open(p_source_file, FileAccess::READ, &err);
	if (!f) {
		ERR_PRINTS("Error opening file '" + p_source_file + "'.");
		return err;
	}
	Ref<Image> image;
	image.instance();
	String abs_path = f->get_path_absolute();
	msdf_output(abs_path.utf8().ptrw(), image, state);
	Ref<ImageTexture> image_texture;
	image_texture.instance();
	image_texture->create_from_image(image);
	String save_path = p_save_path + ".res";
	r_gen_files->push_back(save_path);
	return ResourceSaver::save(save_path, image_texture);
}

ResourceImporterSVGDistanceField::ResourceImporterSVGDistanceField() {
}

ResourceImporterSVGDistanceField::~ResourceImporterSVGDistanceField() {
}
