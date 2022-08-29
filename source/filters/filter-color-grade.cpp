/*
 * Modern effects for a modern Streamer
 * Copyright (C) 2017 Michael Fabian Dirks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "filter-color-grade.hpp"
#include "strings.hpp"
#include "obs/gs/gs-helper.hpp"
#include "util/util-logging.hpp"

#include "warning-disable.hpp"
#include <stdexcept>
#include "warning-enable.hpp"

// OBS
#include "warning-disable.hpp"
extern "C" {
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <util/platform.h>
}
#include "warning-enable.hpp"

#ifdef _DEBUG
#define ST_PREFIX "<%s> "
#define D_LOG_ERROR(x, ...) P_LOG_ERROR(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_WARNING(x, ...) P_LOG_WARN(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_INFO(x, ...) P_LOG_INFO(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_DEBUG(x, ...) P_LOG_DEBUG(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#else
#define ST_PREFIX "<filter::color_grade> "
#define D_LOG_ERROR(...) P_LOG_ERROR(ST_PREFIX __VA_ARGS__)
#define D_LOG_WARNING(...) P_LOG_WARN(ST_PREFIX __VA_ARGS__)
#define D_LOG_INFO(...) P_LOG_INFO(ST_PREFIX __VA_ARGS__)
#define D_LOG_DEBUG(...) P_LOG_DEBUG(ST_PREFIX __VA_ARGS__)
#endif

#define ST_I18N "Filter.ColorGrade"
// Lift
#define ST_KEY_LIFT "Filter.ColorGrade.Lift"
#define ST_I18N_LIFT ST_I18N ".Lift"
#define ST_KEY_LIFT_(x) ST_KEY_LIFT "." x
#define ST_I18N_LIFT_(x) ST_I18N_LIFT "." x
// Gamma
#define ST_KEY_GAMMA "Filter.ColorGrade.Gamma"
#define ST_I18N_GAMMA ST_I18N ".Gamma"
#define ST_KEY_GAMMA_(x) ST_KEY_GAMMA "." x
#define ST_I18N_GAMMA_(x) ST_I18N_GAMMA "." x
// Gain
#define ST_KEY_GAIN "Filter.ColorGrade.Gain"
#define ST_I18N_GAIN ST_I18N ".Gain"
#define ST_KEY_GAIN_(x) ST_KEY_GAIN "." x
#define ST_I18N_GAIN_(x) ST_I18N_GAIN "." x
// Offset
#define ST_KEY_OFFSET "Filter.ColorGrade.Offset"
#define ST_I18N_OFFSET ST_I18N ".Offset"
#define ST_KEY_OFFSET_(x) ST_KEY_OFFSET "." x
#define ST_I18N_OFFSET_(x) ST_I18N_OFFSET "." x
// Tint
#define ST_KEY_TINT "Filter.ColorGrade.Tint"
#define ST_I18N_TINT ST_I18N ".Tint"
#define ST_KEY_TINT_DETECTION ST_KEY_TINT ".Detection"
#define ST_I18N_TINT_DETECTION ST_I18N_TINT ".Detection"
#define ST_I18N_TINT_DETECTION_(x) ST_I18N_TINT_DETECTION "." x
#define ST_KEY_TINT_MODE ST_KEY_TINT ".Mode"
#define ST_I18N_TINT_MODE ST_I18N_TINT ".Mode"
#define ST_I18N_TINT_MODE_(x) ST_I18N_TINT_MODE "." x
#define ST_KEY_TINT_EXPONENT ST_KEY_TINT ".Exponent"
#define ST_I18N_TINT_EXPONENT ST_I18N_TINT ".Exponent"
#define ST_KEY_TINT_(x, y) ST_KEY_TINT "." x "." y
#define ST_I18N_TINT_(x, y) ST_I18N_TINT "." x "." y
// Color Correction
#define ST_KEY_CORRECTION "Filter.ColorGrade.Correction"
#define ST_KEY_CORRECTION_(x) ST_KEY_CORRECTION "." x
#define ST_I18N_CORRECTION ST_I18N ".Correction"
#define ST_I18N_CORRECTION_(x) ST_I18N_CORRECTION "." x
// Render Mode
#define ST_KEY_RENDERMODE "Filter.ColorGrade.RenderMode"
#define ST_I18N_RENDERMODE ST_I18N ".RenderMode"
#define ST_I18N_RENDERMODE_DIRECT ST_I18N_RENDERMODE ".Direct"
#define ST_I18N_RENDERMODE_LUT_2BIT ST_I18N_RENDERMODE ".LUT.2Bit"
#define ST_I18N_RENDERMODE_LUT_4BIT ST_I18N_RENDERMODE ".LUT.4Bit"
#define ST_I18N_RENDERMODE_LUT_6BIT ST_I18N_RENDERMODE ".LUT.6Bit"
#define ST_I18N_RENDERMODE_LUT_8BIT ST_I18N_RENDERMODE ".LUT.8Bit"
#define ST_I18N_RENDERMODE_LUT_10BIT ST_I18N_RENDERMODE ".LUT.10Bit"

#define ST_RED "Red"
#define ST_GREEN "Green"
#define ST_BLUE "Blue"
#define ST_ALL "All"
#define ST_HUE "Hue"
#define ST_SATURATION "Saturation"
#define ST_LIGHTNESS "Lightness"
#define ST_CONTRAST "Contrast"
#define ST_TONE_LOW "Shadow"
#define ST_TONE_MID "Midtone"
#define ST_TONE_HIGH "Highlight"
#define ST_DETECTION_HSV "HSV"
#define ST_DETECTION_HSL "HSL"
#define ST_DETECTION_YUV_SDR "YUV.SDR"
#define ST_MODE_LINEAR "Linear"
#define ST_MODE_EXP "Exp"
#define ST_MODE_EXP2 "Exp2"
#define ST_MODE_LOG "Log"
#define ST_MODE_LOG10 "Log10"

using namespace streamfx::filter::color_grade;

static constexpr std::string_view HELP_URL = "https://github.com/Xaymar/obs-StreamFX/wiki/Filter-Color-Grade";

// TODO: Figure out a way to merge _lut_rt, _lut_texture, _rt_source, _rt_grad, _tex_source, _tex_grade, _source_updated and _grade_updated.
// Seriously this is too much GPU space wasted on unused trash.

color_grade_instance::~color_grade_instance() {}

color_grade_instance::color_grade_instance(obs_data_t* data, obs_source_t* self)
	: obs::source_instance(data, self), _effect(),

	  _lift(), _gamma(), _gain(), _offset(), _tint_detection(), _tint_luma(), _tint_exponent(), _tint_low(),
	  _tint_mid(), _tint_hig(), _correction(), _lut_enabled(true), _lut_depth(),

	  _cache_rt(), _cache_texture(), _cache_fresh(false),

	  _lut_initialized(false), _lut_dirty(true), _lut_producer(), _lut_consumer()
{
	{
		auto gctx = streamfx::obs::gs::context();

		// Load the color grading effect.
		{
			auto file = streamfx::data_file_path("effects/color-grade.effect");
			try {
				_effect = streamfx::obs::gs::effect::create(file);
			} catch (std::exception& ex) {
				D_LOG_ERROR("Error loading '%s': %s", file.u8string().c_str(), ex.what());
				throw;
			}
		}

		// Initialize LUT work flow.
		try {
			_lut_producer    = std::make_shared<streamfx::gfx::lut::producer>();
			_lut_consumer    = std::make_shared<streamfx::gfx::lut::consumer>();
			_lut_initialized = true;
		} catch (std::exception const& ex) {
			D_LOG_WARNING("Failed to initialize LUT rendering, falling back to direct rendering.\n%s", ex.what());
			_lut_initialized = false;
		}

		// Allocate render target for rendering.
		try {
			allocate_rendertarget(GS_RGBA);
		} catch (std::exception const& ex) {
			D_LOG_ERROR("Failed to acquire render target for rendering: %s", ex.what());
			throw;
		}
	}

	update(data);
}

void color_grade_instance::allocate_rendertarget(gs_color_format format)
{
	_cache_rt = std::make_unique<streamfx::obs::gs::rendertarget>(format, GS_ZS_NONE);
}

float_t fix_gamma_value(double_t v)
{
	if (v < 0.0) {
		return static_cast<float_t>(-v + 1.0);
	} else {
		return static_cast<float_t>(1.0 / (v + 1.0));
	}
}

void color_grade_instance::load(obs_data_t* data)
{
	update(data);
}

void color_grade_instance::migrate(obs_data_t* data, uint64_t version) {}

void color_grade_instance::update(obs_data_t* data)
{
	_lift.x         = static_cast<float_t>(obs_data_get_double(data, ST_KEY_LIFT_(ST_RED)) / 100.0);
	_lift.y         = static_cast<float_t>(obs_data_get_double(data, ST_KEY_LIFT_(ST_GREEN)) / 100.0);
	_lift.z         = static_cast<float_t>(obs_data_get_double(data, ST_KEY_LIFT_(ST_BLUE)) / 100.0);
	_lift.w         = static_cast<float_t>(obs_data_get_double(data, ST_KEY_LIFT_(ST_ALL)) / 100.0);
	_gamma.x        = fix_gamma_value(obs_data_get_double(data, ST_KEY_GAMMA_(ST_RED)) / 100.0);
	_gamma.y        = fix_gamma_value(obs_data_get_double(data, ST_KEY_GAMMA_(ST_GREEN)) / 100.0);
	_gamma.z        = fix_gamma_value(obs_data_get_double(data, ST_KEY_GAMMA_(ST_BLUE)) / 100.0);
	_gamma.w        = fix_gamma_value(obs_data_get_double(data, ST_KEY_GAMMA_(ST_ALL)) / 100.0);
	_gain.x         = static_cast<float_t>(obs_data_get_double(data, ST_KEY_GAIN_(ST_RED)) / 100.0);
	_gain.y         = static_cast<float_t>(obs_data_get_double(data, ST_KEY_GAIN_(ST_GREEN)) / 100.0);
	_gain.z         = static_cast<float_t>(obs_data_get_double(data, ST_KEY_GAIN_(ST_BLUE)) / 100.0);
	_gain.w         = static_cast<float_t>(obs_data_get_double(data, ST_KEY_GAIN_(ST_ALL)) / 100.0);
	_offset.x       = static_cast<float_t>(obs_data_get_double(data, ST_KEY_OFFSET_(ST_RED)) / 100.0);
	_offset.y       = static_cast<float_t>(obs_data_get_double(data, ST_KEY_OFFSET_(ST_GREEN)) / 100.0);
	_offset.z       = static_cast<float_t>(obs_data_get_double(data, ST_KEY_OFFSET_(ST_BLUE)) / 100.0);
	_offset.w       = static_cast<float_t>(obs_data_get_double(data, ST_KEY_OFFSET_(ST_ALL)) / 100.0);
	_tint_detection = static_cast<detection_mode>(obs_data_get_int(data, ST_KEY_TINT_DETECTION));
	_tint_luma      = static_cast<luma_mode>(obs_data_get_int(data, ST_KEY_TINT_MODE));
	_tint_exponent  = static_cast<float_t>(obs_data_get_double(data, ST_KEY_TINT_EXPONENT));
	_tint_low.x     = static_cast<float_t>(obs_data_get_double(data, ST_KEY_TINT_(ST_TONE_LOW, ST_RED)) / 100.0);
	_tint_low.y     = static_cast<float_t>(obs_data_get_double(data, ST_KEY_TINT_(ST_TONE_LOW, ST_GREEN)) / 100.0);
	_tint_low.z     = static_cast<float_t>(obs_data_get_double(data, ST_KEY_TINT_(ST_TONE_LOW, ST_BLUE)) / 100.0);
	_tint_mid.x     = static_cast<float_t>(obs_data_get_double(data, ST_KEY_TINT_(ST_TONE_MID, ST_RED)) / 100.0);
	_tint_mid.y     = static_cast<float_t>(obs_data_get_double(data, ST_KEY_TINT_(ST_TONE_MID, ST_GREEN)) / 100.0);
	_tint_mid.z     = static_cast<float_t>(obs_data_get_double(data, ST_KEY_TINT_(ST_TONE_MID, ST_BLUE)) / 100.0);
	_tint_hig.x     = static_cast<float_t>(obs_data_get_double(data, ST_KEY_TINT_(ST_TONE_HIGH, ST_RED)) / 100.0);
	_tint_hig.y     = static_cast<float_t>(obs_data_get_double(data, ST_KEY_TINT_(ST_TONE_HIGH, ST_GREEN)) / 100.0);
	_tint_hig.z     = static_cast<float_t>(obs_data_get_double(data, ST_KEY_TINT_(ST_TONE_HIGH, ST_BLUE)) / 100.0);
	_correction.x   = static_cast<float_t>(obs_data_get_double(data, ST_KEY_CORRECTION_(ST_HUE)) / 360.0);
	_correction.y   = static_cast<float_t>(obs_data_get_double(data, ST_KEY_CORRECTION_(ST_SATURATION)) / 100.0);
	_correction.z   = static_cast<float_t>(obs_data_get_double(data, ST_KEY_CORRECTION_(ST_LIGHTNESS)) / 100.0);
	_correction.w   = static_cast<float_t>(obs_data_get_double(data, ST_KEY_CORRECTION_(ST_CONTRAST)) / 100.0);

	{
		int64_t v = obs_data_get_int(data, ST_KEY_RENDERMODE);

		// LUT status depends on selected option.
		_lut_enabled = v != 0; // 0 (Direct)

		if (v == -1) {
			_lut_depth = streamfx::gfx::lut::color_depth::_8;
		} else if (v > 0) {
			_lut_depth = static_cast<streamfx::gfx::lut::color_depth>(v);
		}
	}

	if (_lut_enabled && _lut_initialized)
		_lut_dirty = true;
}

void color_grade_instance::prepare_effect()
{
	if (auto p = _effect.get_parameter("pLift"); p) {
		p.set_float4(_lift);
	}

	if (auto p = _effect.get_parameter("pGamma"); p) {
		p.set_float4(_gamma);
	}

	if (auto p = _effect.get_parameter("pGain"); p) {
		p.set_float4(_gain);
	}

	if (auto p = _effect.get_parameter("pOffset"); p) {
		p.set_float4(_offset);
	}

	if (auto p = _effect.get_parameter("pLift"); p) {
		p.set_float4(_lift);
	}

	if (auto p = _effect.get_parameter("pTintDetection"); p) {
		p.set_int(static_cast<int32_t>(_tint_detection));
	}

	if (auto p = _effect.get_parameter("pTintMode"); p) {
		p.set_int(static_cast<int32_t>(_tint_luma));
	}

	if (auto p = _effect.get_parameter("pTintExponent"); p) {
		p.set_float(_tint_exponent);
	}

	if (auto p = _effect.get_parameter("pTintLow"); p) {
		p.set_float3(_tint_low);
	}

	if (auto p = _effect.get_parameter("pTintMid"); p) {
		p.set_float3(_tint_mid);
	}

	if (auto p = _effect.get_parameter("pTintHig"); p) {
		p.set_float3(_tint_hig);
	}

	if (auto p = _effect.get_parameter("pCorrection"); p) {
		p.set_float4(_correction);
	}
}

void color_grade_instance::rebuild_lut()
{
#ifdef ENABLE_PROFILING
	streamfx::obs::gs::debug_marker gdm{streamfx::obs::gs::debug_color_cache, "Rebuild LUT"};
#endif

	// Generate a fresh LUT texture.
	auto lut_texture = _lut_producer->produce(_lut_depth);

	// Modify the LUT with our color grade.
	if (lut_texture) {
		// Check if we have a render target to work with and if it's the correct format.
		if (!_lut_rt || (lut_texture->get_color_format() != _lut_rt->get_color_format())) {
			// Create a new render target with new format.
			_lut_rt = std::make_unique<streamfx::obs::gs::rendertarget>(lut_texture->get_color_format(), GS_ZS_NONE);
		}

		// Prepare our color grade effect.
		prepare_effect();

		// Assign texture.
		if (auto p = _effect.get_parameter("image"); p) {
			p.set_texture(lut_texture);
		}

		{ // Begin rendering.
			auto op = _lut_rt->render(lut_texture->get_width(), lut_texture->get_height());

			// Set up graphics context.
			gs_ortho(0, 1, 0, 1, 0, 1);
			gs_blend_state_push();
			gs_enable_blending(false);
			gs_enable_color(true, true, true, true);
			gs_enable_stencil_test(false);
			gs_enable_stencil_write(false);

			while (gs_effect_loop(_effect.get_object(), "Draw")) {
				streamfx::gs_draw_fullscreen_tri();
			}

			gs_blend_state_pop();
		}

		_lut_rt->get_texture(_lut_texture);
		if (!_lut_texture) {
			throw std::runtime_error("Failed to produce modified LUT texture.");
		}
	} else {
		throw std::runtime_error("Failed to produce LUT texture.");
	}

	_lut_dirty = false;
}

void color_grade_instance::video_tick(float)
{
	_ccache_fresh = false;
	_cache_fresh  = false;
}

void color_grade_instance::video_render(gs_effect_t* shader)
{
	// Grab initial values.
	obs_source_t* parent = obs_filter_get_parent(_self);
	obs_source_t* target = obs_filter_get_target(_self);
	uint32_t      width  = obs_source_get_base_width(target);
	uint32_t      height = obs_source_get_base_height(target);
	vec4          blank  = vec4{0, 0, 0, 0};
	shader               = shader ? shader : obs_get_base_effect(OBS_EFFECT_DEFAULT);

	// Skip filter if anything is wrong.
	if (!parent || !target || !width || !height) {
		obs_source_skip_video_filter(_self);
		return;
	}

#ifdef ENABLE_PROFILING
	streamfx::obs::gs::debug_marker gdmp{streamfx::obs::gs::debug_color_source, "Color Grading '%s'",
										 obs_source_get_name(_self)};
#endif

	// TODO: Optimize this once (https://github.com/obsproject/obs-studio/pull/4199) is merged.
	// - We can skip the original capture and reduce the overall impact of this.

	// 1. Capture the filter/source rendered above this.
	if (!_ccache_fresh || !_ccache_texture) {
#ifdef ENABLE_PROFILING
		streamfx::obs::gs::debug_marker gdmp{streamfx::obs::gs::debug_color_cache, "Cache '%s'",
											 obs_source_get_name(target)};
#endif
		// If the input cache render target doesn't exist, create it.
		if (!_ccache_rt) {
			_ccache_rt = std::make_shared<streamfx::obs::gs::rendertarget>(GS_RGBA, GS_ZS_NONE);
		}

		{
			auto op = _ccache_rt->render(width, height);
			gs_ortho(0, static_cast<float_t>(width), 0, static_cast<float_t>(height), 0, 1);

			// Blank out the input cache.
			gs_clear(GS_CLEAR_COLOR | GS_CLEAR_DEPTH, &blank, 0., 0);

			// Begin rendering the actual input source.
			obs_source_process_filter_begin(_self, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING);

			// Enable all colors for rendering.
			gs_enable_color(true, true, true, true);

			// Prevent blending with existing content, even if it is cleared.
			gs_blend_state_push();
			gs_enable_blending(false);
			gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

			// Disable depth testing.
			gs_enable_depth_test(false);

			// Disable stencil testing.
			gs_enable_stencil_test(false);

			// Disable culling.
			gs_set_cull_mode(GS_NEITHER);

			// End rendering the actual input source.
			obs_source_process_filter_end(_self, obs_get_base_effect(OBS_EFFECT_DEFAULT), width, height);

			// Restore original blend mode.
			gs_blend_state_pop();
		}

		// Try and retrieve the input cache as a texture for later use.
		_ccache_rt->get_texture(_ccache_texture);
		if (!_ccache_texture) {
			throw std::runtime_error("Failed to cache original source.");
		}

		// Mark the input cache as valid.
		_ccache_fresh = true;
	}

	// 2. Apply one of the two rendering methods (LUT or Direct).
	if (_lut_initialized && _lut_enabled) { // Try to apply with the LUT based method.
		try {
#ifdef ENABLE_PROFILING
			streamfx::obs::gs::debug_marker gdm{streamfx::obs::gs::debug_color_convert, "LUT Rendering"};
#endif
			// If the LUT was changed, rebuild the LUT first.
			if (_lut_dirty) {
				rebuild_lut();

				// Mark the cache as invalid, since the LUT has been changed.
				_cache_fresh = false;
			}

			// Reallocate the rendertarget if necessary.
			if (_cache_rt->get_color_format() != GS_RGBA) {
				allocate_rendertarget(GS_RGBA);
			}

			if (!_cache_fresh) {
				{ // Render the source to the cache.
					auto op = _cache_rt->render(width, height);
					gs_ortho(0, 1., 0, 1., 0, 1);

					// Blank out the input cache.
					gs_clear(GS_CLEAR_COLOR | GS_CLEAR_DEPTH, &blank, 0., 0);

					// Enable all colors for rendering.
					gs_enable_color(true, true, true, true);

					// Prevent blending with existing content, even if it is cleared.
					gs_blend_state_push();
					gs_enable_blending(false);
					gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

					// Disable depth testing.
					gs_enable_depth_test(false);

					// Disable stencil testing.
					gs_enable_stencil_test(false);

					// Disable culling.
					gs_set_cull_mode(GS_NEITHER);

					auto effect = _lut_consumer->prepare(_lut_depth, _lut_texture);
					effect->get_parameter("image").set_texture(_ccache_texture);
					while (gs_effect_loop(effect->get_object(), "Draw")) {
						streamfx::gs_draw_fullscreen_tri();
					}

					// Restore original blend mode.
					gs_blend_state_pop();
				}

				// Try and retrieve the render cache as a texture.
				_cache_rt->get_texture(_cache_texture);

				// Mark the render cache as valid.
				_cache_fresh = true;
			}
		} catch (std::exception const& ex) {
			// If anything happened, revert to direct rendering.
			_lut_rt.reset();
			_lut_texture.reset();
			_lut_enabled = false;
			D_LOG_WARNING("Reverting to direct rendering due to error: %s", ex.what());
		}
	}
	if ((!_lut_initialized || !_lut_enabled) && !_cache_fresh) {
#ifdef ENABLE_PROFILING
		streamfx::obs::gs::debug_marker gdm{streamfx::obs::gs::debug_color_convert, "Direct Rendering"};
#endif
		// Reallocate the rendertarget if necessary.
		if (_cache_rt->get_color_format() != GS_RGBA) {
			allocate_rendertarget(GS_RGBA);
		}

		{ // Render the source to the cache.
			auto op = _cache_rt->render(width, height);
			gs_ortho(0, 1, 0, 1, 0, 1);

			prepare_effect();

			// Blank out the input cache.
			gs_clear(GS_CLEAR_COLOR | GS_CLEAR_DEPTH, &blank, 0., 0);

			// Enable all colors for rendering.
			gs_enable_color(true, true, true, true);

			// Prevent blending with existing content, even if it is cleared.
			gs_blend_state_push();
			gs_enable_blending(false);
			gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

			// Disable depth testing.
			gs_enable_depth_test(false);

			// Disable stencil testing.
			gs_enable_stencil_test(false);

			// Disable culling.
			gs_set_cull_mode(GS_NEITHER);

			// Render the effect.
			_effect.get_parameter("image").set_texture(_ccache_texture);
			while (gs_effect_loop(_effect.get_object(), "Draw")) {
				streamfx::gs_draw_fullscreen_tri();
			}

			// Restore original blend mode.
			gs_blend_state_pop();
		}

		// Try and retrieve the render cache as a texture.
		_cache_rt->get_texture(_cache_texture);

		// Mark the render cache as valid.
		_cache_fresh = true;
	}
	if (!_cache_texture) {
		throw std::runtime_error("Failed to cache processed source.");
	}

	// 3. Render the output cache.
	{
#ifdef ENABLE_PROFILING
		streamfx::obs::gs::debug_marker gdm{streamfx::obs::gs::debug_color_cache_render, "Draw Cache"};
#endif
		// Revert GPU status to what OBS Studio expects.
		gs_enable_depth_test(false);
		gs_enable_color(true, true, true, true);
		gs_set_cull_mode(GS_NEITHER);

		// Draw the render cache.
		while (gs_effect_loop(shader, "Draw")) {
			gs_effect_set_texture(gs_effect_get_param_by_name(shader, "image"),
								  _cache_texture ? _cache_texture->get_object() : nullptr);
			gs_draw_sprite(nullptr, 0, width, height);
		}
	}
}

color_grade_factory::color_grade_factory()
{
	_info.id           = S_PREFIX "filter-color-grade";
	_info.type         = OBS_SOURCE_TYPE_FILTER;
	_info.output_flags = OBS_SOURCE_VIDEO;

	support_size(false);
	finish_setup();
	register_proxy("obs-stream-effects-filter-color-grade");
}

color_grade_factory::~color_grade_factory() {}

const char* color_grade_factory::get_name()
{
	return D_TRANSLATE(ST_I18N);
}

void color_grade_factory::get_defaults2(obs_data_t* data)
{
	obs_data_set_default_double(data, ST_KEY_LIFT_(ST_RED), 0);
	obs_data_set_default_double(data, ST_KEY_LIFT_(ST_GREEN), 0);
	obs_data_set_default_double(data, ST_KEY_LIFT_(ST_BLUE), 0);
	obs_data_set_default_double(data, ST_KEY_LIFT_(ST_ALL), 0);
	obs_data_set_default_double(data, ST_KEY_GAMMA_(ST_RED), 0);
	obs_data_set_default_double(data, ST_KEY_GAMMA_(ST_GREEN), 0);
	obs_data_set_default_double(data, ST_KEY_GAMMA_(ST_BLUE), 0);
	obs_data_set_default_double(data, ST_KEY_GAMMA_(ST_ALL), 0);
	obs_data_set_default_double(data, ST_KEY_GAIN_(ST_RED), 100.0);
	obs_data_set_default_double(data, ST_KEY_GAIN_(ST_GREEN), 100.0);
	obs_data_set_default_double(data, ST_KEY_GAIN_(ST_BLUE), 100.0);
	obs_data_set_default_double(data, ST_KEY_GAIN_(ST_ALL), 100.0);
	obs_data_set_default_double(data, ST_KEY_OFFSET_(ST_RED), 0.0);
	obs_data_set_default_double(data, ST_KEY_OFFSET_(ST_GREEN), 0.0);
	obs_data_set_default_double(data, ST_KEY_OFFSET_(ST_BLUE), 0.0);
	obs_data_set_default_double(data, ST_KEY_OFFSET_(ST_ALL), 0.0);
	obs_data_set_default_int(data, ST_KEY_TINT_MODE, static_cast<int64_t>(luma_mode::Linear));
	obs_data_set_default_int(data, ST_KEY_TINT_DETECTION, static_cast<int64_t>(detection_mode::YUV_SDR));
	obs_data_set_default_double(data, ST_KEY_TINT_EXPONENT, 1.5);
	obs_data_set_default_double(data, ST_KEY_TINT_(ST_TONE_LOW, ST_RED), 100.0);
	obs_data_set_default_double(data, ST_KEY_TINT_(ST_TONE_LOW, ST_GREEN), 100.0);
	obs_data_set_default_double(data, ST_KEY_TINT_(ST_TONE_LOW, ST_BLUE), 100.0);
	obs_data_set_default_double(data, ST_KEY_TINT_(ST_TONE_MID, ST_RED), 100.0);
	obs_data_set_default_double(data, ST_KEY_TINT_(ST_TONE_MID, ST_GREEN), 100.0);
	obs_data_set_default_double(data, ST_KEY_TINT_(ST_TONE_MID, ST_BLUE), 100.0);
	obs_data_set_default_double(data, ST_KEY_TINT_(ST_TONE_HIGH, ST_RED), 100.0);
	obs_data_set_default_double(data, ST_KEY_TINT_(ST_TONE_HIGH, ST_GREEN), 100.0);
	obs_data_set_default_double(data, ST_KEY_TINT_(ST_TONE_HIGH, ST_BLUE), 100.0);
	obs_data_set_default_double(data, ST_KEY_CORRECTION_(ST_HUE), 0.0);
	obs_data_set_default_double(data, ST_KEY_CORRECTION_(ST_SATURATION), 100.0);
	obs_data_set_default_double(data, ST_KEY_CORRECTION_(ST_LIGHTNESS), 100.0);
	obs_data_set_default_double(data, ST_KEY_CORRECTION_(ST_CONTRAST), 100.0);

	obs_data_set_default_int(data, ST_KEY_RENDERMODE, -1);
}

obs_properties_t* color_grade_factory::get_properties2(color_grade_instance* data)
{
	obs_properties_t* pr = obs_properties_create();

#ifdef ENABLE_FRONTEND
	{
		obs_properties_add_button2(pr, S_MANUAL_OPEN, D_TRANSLATE(S_MANUAL_OPEN),
								   streamfx::filter::color_grade::color_grade_factory::on_manual_open, nullptr);
	}
#endif

	{
		obs_properties_t* grp = obs_properties_create();
		obs_properties_add_group(pr, ST_KEY_LIFT, D_TRANSLATE(ST_I18N_LIFT), OBS_GROUP_NORMAL, grp);

		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_LIFT_(ST_RED), D_TRANSLATE(ST_I18N_LIFT_(ST_RED)),
													 -1000., 100., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_LIFT_(ST_GREEN), D_TRANSLATE(ST_I18N_LIFT_(ST_GREEN)),
													 -1000., 100., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_LIFT_(ST_BLUE), D_TRANSLATE(ST_I18N_LIFT_(ST_BLUE)),
													 -1000., 100., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_LIFT_(ST_ALL), D_TRANSLATE(ST_I18N_LIFT_(ST_ALL)),
													 -1000., 100., .01);
			obs_property_float_set_suffix(p, " %");
		}
	}

	{
		obs_properties_t* grp = obs_properties_create();
		obs_properties_add_group(pr, ST_KEY_GAMMA, D_TRANSLATE(ST_I18N_GAMMA), OBS_GROUP_NORMAL, grp);

		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_GAMMA_(ST_RED), D_TRANSLATE(ST_I18N_GAMMA_(ST_RED)),
													 -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_GAMMA_(ST_GREEN),
													 D_TRANSLATE(ST_I18N_GAMMA_(ST_GREEN)), -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_GAMMA_(ST_BLUE), D_TRANSLATE(ST_I18N_GAMMA_(ST_BLUE)),
													 -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_GAMMA_(ST_ALL), D_TRANSLATE(ST_I18N_GAMMA_(ST_ALL)),
													 -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
	}

	{
		obs_properties_t* grp = obs_properties_create();
		obs_properties_add_group(pr, ST_KEY_GAIN, D_TRANSLATE(ST_I18N_GAIN), OBS_GROUP_NORMAL, grp);

		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_GAIN_(ST_RED), D_TRANSLATE(ST_I18N_GAIN_(ST_RED)),
													 -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_GAIN_(ST_GREEN), D_TRANSLATE(ST_I18N_GAIN_(ST_GREEN)),
													 -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_GAIN_(ST_BLUE), D_TRANSLATE(ST_I18N_GAIN_(ST_BLUE)),
													 -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_GAIN_(ST_ALL), D_TRANSLATE(ST_I18N_GAIN_(ST_ALL)),
													 -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
	}

	{
		obs_properties_t* grp = obs_properties_create();
		obs_properties_add_group(pr, ST_KEY_OFFSET, D_TRANSLATE(ST_I18N_OFFSET), OBS_GROUP_NORMAL, grp);

		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_OFFSET_(ST_RED), D_TRANSLATE(ST_I18N_OFFSET_(ST_RED)),
													 -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_OFFSET_(ST_GREEN),
													 D_TRANSLATE(ST_I18N_OFFSET_(ST_GREEN)), -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_OFFSET_(ST_BLUE),
													 D_TRANSLATE(ST_I18N_OFFSET_(ST_BLUE)), -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_OFFSET_(ST_ALL), D_TRANSLATE(ST_I18N_OFFSET_(ST_ALL)),
													 -1000., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
	}

	{
		obs_properties_t* grp = obs_properties_create();
		obs_properties_add_group(pr, ST_KEY_TINT, D_TRANSLATE(ST_I18N_TINT), OBS_GROUP_NORMAL, grp);

		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_TINT_(ST_TONE_LOW, ST_RED),
													 D_TRANSLATE(ST_I18N_TINT_(ST_TONE_LOW, ST_RED)), 0, 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_TINT_(ST_TONE_LOW, ST_GREEN),
													 D_TRANSLATE(ST_I18N_TINT_(ST_TONE_LOW, ST_GREEN)), 0, 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_TINT_(ST_TONE_LOW, ST_BLUE),
													 D_TRANSLATE(ST_I18N_TINT_(ST_TONE_LOW, ST_BLUE)), 0, 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}

		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_TINT_(ST_TONE_MID, ST_RED),
													 D_TRANSLATE(ST_I18N_TINT_(ST_TONE_MID, ST_RED)), 0, 1000., 0.01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_TINT_(ST_TONE_MID, ST_GREEN),
													 D_TRANSLATE(ST_I18N_TINT_(ST_TONE_MID, ST_GREEN)), 0, 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_TINT_(ST_TONE_MID, ST_BLUE),
													 D_TRANSLATE(ST_I18N_TINT_(ST_TONE_MID, ST_BLUE)), 0, 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}

		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_TINT_(ST_TONE_HIGH, ST_RED),
													 D_TRANSLATE(ST_I18N_TINT_(ST_TONE_HIGH, ST_RED)), 0, 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_TINT_(ST_TONE_HIGH, ST_GREEN),
													 D_TRANSLATE(ST_I18N_TINT_(ST_TONE_HIGH, ST_GREEN)), 0, 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_TINT_(ST_TONE_HIGH, ST_BLUE),
													 D_TRANSLATE(ST_I18N_TINT_(ST_TONE_HIGH, ST_BLUE)), 0, 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
	}

	{
		obs_properties_t* grp = obs_properties_create();
		obs_properties_add_group(pr, ST_KEY_CORRECTION, D_TRANSLATE(ST_I18N_CORRECTION), OBS_GROUP_NORMAL, grp);

		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_CORRECTION_(ST_HUE),
													 D_TRANSLATE(ST_I18N_CORRECTION_(ST_HUE)), -180., 180., .01);
			obs_property_float_set_suffix(p, " °");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_CORRECTION_(ST_SATURATION),
													 D_TRANSLATE(ST_I18N_CORRECTION_(ST_SATURATION)), 0., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_CORRECTION_(ST_LIGHTNESS),
													 D_TRANSLATE(ST_I18N_CORRECTION_(ST_LIGHTNESS)), 0., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
		{
			auto p = obs_properties_add_float_slider(grp, ST_KEY_CORRECTION_(ST_CONTRAST),
													 D_TRANSLATE(ST_I18N_CORRECTION_(ST_CONTRAST)), 0., 1000., .01);
			obs_property_float_set_suffix(p, " %");
		}
	}

	{
		obs_properties_t* grp = obs_properties_create();
		obs_properties_add_group(pr, S_ADVANCED, D_TRANSLATE(S_ADVANCED), OBS_GROUP_NORMAL, grp);

		{
			auto p = obs_properties_add_list(grp, ST_KEY_TINT_MODE, D_TRANSLATE(ST_I18N_TINT_MODE), OBS_COMBO_TYPE_LIST,
											 OBS_COMBO_FORMAT_INT);
			std::pair<const char*, luma_mode> els[] = {{ST_I18N_TINT_MODE_(ST_MODE_LINEAR), luma_mode::Linear},
													   {ST_I18N_TINT_MODE_(ST_MODE_EXP), luma_mode::Exp},
													   {ST_I18N_TINT_MODE_(ST_MODE_EXP2), luma_mode::Exp2},
													   {ST_I18N_TINT_MODE_(ST_MODE_LOG), luma_mode::Log},
													   {ST_I18N_TINT_MODE_(ST_MODE_LOG10), luma_mode::Log10}};
			for (auto kv : els) {
				obs_property_list_add_int(p, D_TRANSLATE(kv.first), static_cast<int64_t>(kv.second));
			}
		}

		{
			auto p = obs_properties_add_list(grp, ST_KEY_TINT_DETECTION, D_TRANSLATE(ST_I18N_TINT_DETECTION),
											 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
			std::pair<const char*, detection_mode> els[] = {
				{ST_I18N_TINT_DETECTION_(ST_DETECTION_HSV), detection_mode::HSV},
				{ST_I18N_TINT_DETECTION_(ST_DETECTION_HSL), detection_mode::HSL},
				{ST_I18N_TINT_DETECTION_(ST_DETECTION_YUV_SDR), detection_mode::YUV_SDR}};
			for (auto kv : els) {
				obs_property_list_add_int(p, D_TRANSLATE(kv.first), static_cast<int64_t>(kv.second));
			}
		}

		obs_properties_add_float_slider(grp, ST_KEY_TINT_EXPONENT, D_TRANSLATE(ST_I18N_TINT_EXPONENT), 0., 10., .01);

		{
			auto p = obs_properties_add_list(grp, ST_KEY_RENDERMODE, D_TRANSLATE(ST_I18N_RENDERMODE),
											 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
			std::pair<const char*, int64_t> els[] = {
				{S_STATE_AUTOMATIC, -1},
				{ST_I18N_RENDERMODE_DIRECT, 0},
				{ST_I18N_RENDERMODE_LUT_2BIT, static_cast<int64_t>(streamfx::gfx::lut::color_depth::_2)},
				{ST_I18N_RENDERMODE_LUT_4BIT, static_cast<int64_t>(streamfx::gfx::lut::color_depth::_4)},
				{ST_I18N_RENDERMODE_LUT_6BIT, static_cast<int64_t>(streamfx::gfx::lut::color_depth::_6)},
				{ST_I18N_RENDERMODE_LUT_8BIT, static_cast<int64_t>(streamfx::gfx::lut::color_depth::_8)},
				//{ST_RENDERMODE_LUT_10BIT, static_cast<int64_t>(gfx::lut::color_depth::_10)},
			};
			for (auto kv : els) {
				obs_property_list_add_int(p, D_TRANSLATE(kv.first), kv.second);
			}
		}
	}

	return pr;
}

#ifdef ENABLE_FRONTEND
bool color_grade_factory::on_manual_open(obs_properties_t* props, obs_property_t* property, void* data)
{
	try {
		streamfx::open_url(HELP_URL);
		return false;
	} catch (const std::exception& ex) {
		D_LOG_ERROR("Failed to open manual due to error: %s", ex.what());
		return false;
	} catch (...) {
		D_LOG_ERROR("Failed to open manual due to unknown error.", "");
		return false;
	}
}
#endif

std::shared_ptr<color_grade_factory> _color_grade_factory_instance = nullptr;

void streamfx::filter::color_grade::color_grade_factory::initialize()
{
	try {
		if (!_color_grade_factory_instance)
			_color_grade_factory_instance = std::make_shared<color_grade_factory>();
	} catch (const std::exception& ex) {
		D_LOG_ERROR("Failed to initialize due to error: %s", ex.what());
	} catch (...) {
		D_LOG_ERROR("Failed to initialize due to unknown error.", "");
	}
}

void streamfx::filter::color_grade::color_grade_factory::finalize()
{
	_color_grade_factory_instance.reset();
}

std::shared_ptr<color_grade_factory> streamfx::filter::color_grade::color_grade_factory::get()
{
	return _color_grade_factory_instance;
}
