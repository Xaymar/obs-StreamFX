// Modern effects for a modern Streamer
// Copyright (C) 2017 Michael Fabian Dirks
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

#include "gfx-shader.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "obs/gs/gs-helper.hpp"
#include "obs/obs-tools.hpp"
#include "plugin.hpp"

#define ST_I18N "Shader"
#define ST_I18N_REFRESH ST_I18N ".Refresh"
#define ST_KEY_REFRESH "Shader.Refresh"
#define ST_I18N_SHADER ST_I18N ".Shader"
#define ST_KEY_SHADER "Shader.Shader"
#define ST_I18N_SHADER_FILE ST_I18N_SHADER ".File"
#define ST_KEY_SHADER_FILE ST_KEY_SHADER ".File"
#define ST_I18N_SHADER_TECHNIQUE ST_I18N_SHADER ".Technique"
#define ST_KEY_SHADER_TECHNIQUE ST_KEY_SHADER ".Technique"
#define ST_I18N_SHADER_SIZE ST_I18N_SHADER ".Size"
#define ST_KEY_SHADER_SIZE ST_KEY_SHADER ".Size"
#define ST_I18N_SHADER_SIZE_WIDTH ST_I18N_SHADER_SIZE ".Width"
#define ST_KEY_SHADER_SIZE_WIDTH ST_KEY_SHADER_SIZE ".Width"
#define ST_I18N_SHADER_SIZE_HEIGHT ST_I18N_SHADER_SIZE ".Height"
#define ST_KEY_SHADER_SIZE_HEIGHT ST_KEY_SHADER_SIZE ".Height"
#define ST_I18N_SHADER_SEED ST_I18N_SHADER ".Seed"
#define ST_KEY_SHADER_SEED ST_KEY_SHADER ".Seed"
#define ST_I18N_PARAMETERS ST_I18N ".Parameters"
#define ST_KEY_PARAMETERS "Shader.Parameters"

streamfx::gfx::shader::shader::shader(obs_source_t* self, shader_mode mode)
	: _self(self), _mode(mode), _base_width(1), _base_height(1), _active(true),

	  _shader(), _shader_file(), _shader_tech("Draw"), _shader_file_mt(), _shader_file_sz(), _shader_file_tick(0),

	  _width_type(size_type::Percent), _width_value(1.0), _height_type(size_type::Percent), _height_value(1.0),

	  _have_current_params(false), _time(0), _time_loop(0), _loops(0), _random(), _random_seed(0),

	  _rt_up_to_date(false), _rt(std::make_shared<streamfx::obs::gs::rendertarget>(GS_RGBA_UNORM, GS_ZS_NONE))
{
	// Initialize random values.
	_random.seed(static_cast<unsigned long long>(_random_seed));
	for (size_t idx = 0; idx < 16; idx++) {
		_random_values[idx] =
			static_cast<float_t>(static_cast<double_t>(_random()) / static_cast<double_t>(_random.max()));
	}
}

streamfx::gfx::shader::shader::~shader() {}

bool streamfx::gfx::shader::shader::is_shader_different(const std::filesystem::path& file)
try {
	if (std::filesystem::exists(file)) {
		// Check if the file name differs.
		if (file != _shader_file)
			return true;
	}

	if (std::filesystem::exists(_shader_file)) {
		// Is the file write time different?
		if (std::filesystem::last_write_time(_shader_file) != _shader_file_mt)
			return true;

		// Is the file size different?
		if (std::filesystem::file_size(_shader_file) != _shader_file_sz)
			return true;
	}

	return false;
} catch (const std::exception& ex) {
	DLOG_ERROR("Loading shader '%s' failed with error: %s", file.c_str(), ex.what());
	return false;
}

bool streamfx::gfx::shader::shader::is_technique_different(const std::string& tech)
{
	// Is the technique different?
	if (tech != _shader_tech)
		return true;

	return false;
}

bool streamfx::gfx::shader::shader::load_shader(const std::filesystem::path& file, const std::string& tech,
												bool& shader_dirty, bool& param_dirty)
try {
	if (!std::filesystem::exists(file))
		return false;

	shader_dirty = is_shader_different(file);
	param_dirty  = is_technique_different(tech) || shader_dirty;

	// Update Shader
	if (shader_dirty) {
		_shader           = streamfx::obs::gs::effect(file);
		_shader_file_mt   = std::filesystem::last_write_time(file);
		_shader_file_sz   = std::filesystem::file_size(file);
		_shader_file      = file;
		_shader_file_tick = 0;
	}

	// Update Params
	if (param_dirty) {
		auto settings =
			std::shared_ptr<obs_data_t>(obs_source_get_settings(_self), [](obs_data_t* p) { obs_data_release(p); });

		bool have_valid_tech = false;
		for (std::size_t idx = 0; idx < _shader.count_techniques(); idx++) {
			if (_shader.get_technique(idx).name() == tech) {
				have_valid_tech = true;
				break;
			}
		}
		if (have_valid_tech) {
			_shader_tech = tech;
		} else {
			_shader_tech = _shader.get_technique(0).name();

			// Update source data.
			obs_data_set_string(settings.get(), ST_KEY_SHADER_TECHNIQUE, _shader_tech.c_str());
		}

		// Clear the shader parameters map and rebuild.
		_shader_params.clear();
		auto etech = _shader.get_technique(_shader_tech);
		for (std::size_t idx = 0; idx < etech.count_passes(); idx++) {
			auto pass         = etech.get_pass(idx);
			auto fetch_params = [&](std::size_t                                                     count,
									std::function<streamfx::obs::gs::effect_parameter(std::size_t)> get_func) {
				for (std::size_t vidx = 0; vidx < count; vidx++) {
					auto el = get_func(vidx);
					if (!el)
						continue;

					auto el_name = el.get_name();
					auto fnd     = _shader_params.find(el_name);
					if (fnd != _shader_params.end())
						continue;

					auto param = streamfx::gfx::shader::parameter::make_parameter(this, el, ST_KEY_PARAMETERS);

					if (param) {
						_shader_params.insert_or_assign(el_name, param);
						param->defaults(settings.get());
						param->update(settings.get());
					}
				}
			};

			auto gvp = [&](std::size_t idx) { return pass.get_vertex_parameter(idx); };
			fetch_params(pass.count_vertex_parameters(), gvp);
			auto gpp = [&](std::size_t idx) { return pass.get_pixel_parameter(idx); };
			fetch_params(pass.count_pixel_parameters(), gpp);
		}
	}

	return true;
} catch (const std::exception& ex) {
	DLOG_ERROR("Loading shader '%s' failed with error: %s", file.c_str(), ex.what());
	return false;
} catch (...) {
	return false;
}

void streamfx::gfx::shader::shader::defaults(obs_data_t* data)
{
	obs_data_set_default_string(data, ST_KEY_SHADER_FILE, "");
	obs_data_set_default_string(data, ST_KEY_SHADER_TECHNIQUE, "");
	obs_data_set_default_string(data, ST_KEY_SHADER_SIZE_WIDTH, "100.0 %");
	obs_data_set_default_string(data, ST_KEY_SHADER_SIZE_HEIGHT, "100.0 %");
	obs_data_set_default_int(data, ST_KEY_SHADER_SEED, static_cast<long long>(time(NULL)));
}

void streamfx::gfx::shader::shader::properties(obs_properties_t* pr)
{
	_have_current_params = false;

	{
		auto grp = obs_properties_create();
		obs_properties_add_group(pr, ST_KEY_SHADER, D_TRANSLATE(ST_I18N_SHADER), OBS_GROUP_NORMAL, grp);

		{
			std::string path = "";
			if (_shader_file.has_parent_path()) {
				path = _shader_file.parent_path().string();
			} else {
				path = streamfx::data_file_path("examples/").u8string();
			}
			auto p = obs_properties_add_path(grp, ST_KEY_SHADER_FILE, D_TRANSLATE(ST_I18N_SHADER_FILE), OBS_PATH_FILE,
											 "*.*", path.c_str());
		}
		{
			auto p = obs_properties_add_list(grp, ST_KEY_SHADER_TECHNIQUE, D_TRANSLATE(ST_I18N_SHADER_TECHNIQUE),
											 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
		}

		{
			obs_properties_add_button2(
				grp, ST_KEY_REFRESH, D_TRANSLATE(ST_I18N_REFRESH),
				[](obs_properties_t* props, obs_property_t* prop, void* priv) {
					return reinterpret_cast<streamfx::gfx::shader::shader*>(priv)->on_refresh_properties(props, prop);
				},
				this);
		}

		if (_mode != shader_mode::Transition) {
			auto grp2 = obs_properties_create();
			obs_properties_add_group(grp, ST_KEY_SHADER_SIZE, D_TRANSLATE(ST_I18N_SHADER_SIZE), OBS_GROUP_NORMAL, grp2);

			{
				auto p = obs_properties_add_text(grp2, ST_KEY_SHADER_SIZE_WIDTH, D_TRANSLATE(ST_I18N_SHADER_SIZE_WIDTH),
												 OBS_TEXT_DEFAULT);
			}
			{
				auto p = obs_properties_add_text(grp2, ST_KEY_SHADER_SIZE_HEIGHT,
												 D_TRANSLATE(ST_I18N_SHADER_SIZE_HEIGHT), OBS_TEXT_DEFAULT);
			}
		}

		{
			auto p = obs_properties_add_int_slider(grp, ST_KEY_SHADER_SEED, D_TRANSLATE(ST_I18N_SHADER_SEED),
												   std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 1);
		}
	}
	{
		auto grp = obs_properties_create();
		obs_properties_add_group(pr, ST_KEY_PARAMETERS, D_TRANSLATE(ST_I18N_PARAMETERS), OBS_GROUP_NORMAL, grp);
	}

	// Manually call the refresh.
	on_refresh_properties(pr, nullptr);
}

bool streamfx::gfx::shader::shader::on_refresh_properties(obs_properties_t* props, obs_property_t* prop)
{
	if (_shader) { // Clear list of techniques and rebuild it.
		obs_property_t* p_tech_list = obs_properties_get(props, ST_KEY_SHADER_TECHNIQUE);
		obs_property_list_clear(p_tech_list);
		for (std::size_t idx = 0; idx < _shader.count_techniques(); idx++) {
			auto tech = _shader.get_technique(idx);
			obs_property_list_add_string(p_tech_list, tech.name().c_str(), tech.name().c_str());
		}
	}
	{ // Clear parameter options.
		auto grp = obs_property_group_content(obs_properties_get(props, ST_KEY_PARAMETERS));
		for (auto p = obs_properties_first(grp); p != nullptr; p = obs_properties_first(grp)) {
			streamfx::obs::tools::obs_properties_remove_by_name(grp, obs_property_name(p));
		}

		// Rebuild new parameters.
		obs_data_t* data = obs_source_get_settings(_self);
		for (auto kv : _shader_params) {
			try {
				kv.second->properties(grp, data);
				kv.second->defaults(data);
				kv.second->update(data);
			} catch (...) {
				// ToDo: Do something with these?
			}
		}
		obs_source_update(_self, data);
	}

	return true;
}

bool streamfx::gfx::shader::shader::on_shader_or_technique_modified(obs_properties_t* props, obs_property_t* prop,
																	obs_data_t* data)
{
	bool shader_dirty = false;
	bool param_dirty  = false;

	if (!update_shader(data, shader_dirty, param_dirty))
		return false;

	{ // Clear list of techniques and rebuild it.
		obs_property_t* p_tech_list = obs_properties_get(props, ST_KEY_SHADER_TECHNIQUE);
		obs_property_list_clear(p_tech_list);
		for (std::size_t idx = 0; idx < _shader.count_techniques(); idx++) {
			auto tech = _shader.get_technique(idx);
			obs_property_list_add_string(p_tech_list, tech.name().c_str(), tech.name().c_str());
		}
	}
	if (param_dirty || !_have_current_params) {
		// Clear parameter options.
		auto grp = obs_property_group_content(obs_properties_get(props, ST_KEY_PARAMETERS));
		for (auto p = obs_properties_first(grp); p != nullptr; p = obs_properties_first(grp)) {
			streamfx::obs::tools::obs_properties_remove_by_name(grp, obs_property_name(p));
		}

		// Rebuild new parameters.
		for (auto kv : _shader_params) {
			kv.second->properties(grp, data);
			kv.second->defaults(data);
			kv.second->update(data);
		}
	}

	_have_current_params = true;
	return shader_dirty || param_dirty || !_have_current_params;
}

bool streamfx::gfx::shader::shader::update_shader(obs_data_t* data, bool& shader_dirty, bool& param_dirty)
{
	const char* file_c = obs_data_get_string(data, ST_KEY_SHADER_FILE);
	std::string file   = file_c ? file_c : "";
	const char* tech_c = obs_data_get_string(data, ST_KEY_SHADER_TECHNIQUE);
	std::string tech   = tech_c ? tech_c : "Draw";

	return load_shader(file, tech, shader_dirty, param_dirty);
}

inline std::pair<streamfx::gfx::shader::size_type, double_t> parse_text_as_size(const char* text)
{
	double_t v = 0;
	if (sscanf(text, "%lf", &v) == 1) {
		const char* prc_chr = strrchr(text, '%');
		if (prc_chr && (*prc_chr == '%')) {
			return {streamfx::gfx::shader::size_type::Percent, v / 100.0};
		} else {
			return {streamfx::gfx::shader::size_type::Pixel, v};
		}
	} else {
		return {streamfx::gfx::shader::size_type::Percent, 1.0};
	}
}

void streamfx::gfx::shader::shader::update(obs_data_t* data)
{
	bool v1, v2;
	update_shader(data, v1, v2);

	{
		auto sz_x    = parse_text_as_size(obs_data_get_string(data, ST_KEY_SHADER_SIZE_WIDTH));
		_width_type  = sz_x.first;
		_width_value = std::clamp(sz_x.second, 0.01, 8192.0);

		auto sz_y     = parse_text_as_size(obs_data_get_string(data, ST_KEY_SHADER_SIZE_HEIGHT));
		_height_type  = sz_y.first;
		_height_value = std::clamp(sz_y.second, 0.01, 8192.0);
	}

	if (int32_t seed = static_cast<int32_t>(obs_data_get_int(data, ST_KEY_SHADER_SEED)); _random_seed != seed) {
		_random_seed = seed;
		_random.seed(static_cast<unsigned long long>(_random_seed));
		for (size_t idx = 0; idx < 16; idx++) {
			_random_values[idx] =
				static_cast<float_t>(static_cast<double_t>(_random()) / static_cast<double_t>(_random.max()));
		}
	}

	for (auto kv : _shader_params) {
		kv.second->update(data);
	}
}

uint32_t streamfx::gfx::shader::shader::width()
{
	switch (_mode) {
	case shader_mode::Transition:
		return _base_width;
	case shader_mode::Source:
	case shader_mode::Filter:
		switch (_width_type) {
		case size_type::Pixel:
			return std::clamp(static_cast<uint32_t>(_width_value), 1u, 16384u);
		case size_type::Percent:
			return std::clamp(static_cast<uint32_t>(_width_value * _base_width), 1u, 16384u);
		}
	default:
		return 0;
	}
}

uint32_t streamfx::gfx::shader::shader::height()
{
	switch (_mode) {
	case shader_mode::Transition:
		return _base_height;
	case shader_mode::Source:
	case shader_mode::Filter:
		switch (_height_type) {
		case size_type::Pixel:
			return std::clamp(static_cast<uint32_t>(_height_value), 1u, 16384u);
		case size_type::Percent:
			return std::clamp(static_cast<uint32_t>(_height_value * _base_height), 1u, 16384u);
		}
	default:
		return 0;
	}
}

uint32_t streamfx::gfx::shader::shader::base_width()
{
	return _base_width;
}

uint32_t streamfx::gfx::shader::shader::base_height()
{
	return _base_height;
}

bool streamfx::gfx::shader::shader::tick(float_t time)
{
	_shader_file_tick = static_cast<float_t>(static_cast<double_t>(_shader_file_tick) + static_cast<double_t>(time));
	if (_shader_file_tick >= 1.0f / 3.0f) {
		_shader_file_tick -= 1.0f / 3.0f;
		bool v1, v2;
		load_shader(_shader_file, _shader_tech, v1, v2);
	}

	// Update State
	_time += time;
	_time_loop += time;
	if (_time_loop > 1.) {
		_time_loop -= 1.;

		// Loops
		_loops += 1;
		if (_loops >= 4194304)
			_loops = -_loops;
	}

	// Recreate Per-Activation-Random values.
	for (size_t idx = 0; idx < 8; idx++) {
		_random_values[8 + idx] =
			static_cast<float_t>(static_cast<double_t>(_random()) / static_cast<double_t>(_random.max()));
	}

	// Flag Render Target as outdated.
	_rt_up_to_date = false;

	return false;
}

void streamfx::gfx::shader::shader::prepare_render()
{
	if (!_shader)
		return;

	// Assign user parameters
	for (auto kv : _shader_params) {
		kv.second->assign();
	}

	// float4 Time: (Time in Seconds), (Time in Current Second), (Time in Seconds only), (Random Value)
	if (streamfx::obs::gs::effect_parameter el = _shader.get_parameter("Time"); el != nullptr) {
		if (el.get_type() == streamfx::obs::gs::effect_parameter::type::Float4) {
			el.set_float4(
				_time, _time_loop, static_cast<float_t>(_loops),
				static_cast<float_t>(static_cast<double_t>(_random()) / static_cast<double_t>(_random.max())));
		}
	}

	// float4 ViewSize: (Width), (Height), (1.0 / Width), (1.0 / Height)
	if (auto el = _shader.get_parameter("ViewSize"); el != nullptr) {
		if (el.get_type() == streamfx::obs::gs::effect_parameter::type::Float4) {
			el.set_float4(static_cast<float_t>(width()), static_cast<float_t>(height()),
						  1.0f / static_cast<float_t>(width()), 1.0f / static_cast<float_t>(height()));
		}
	}

	// float4x4 Random: float4[Per-Instance Random], float4[Per-Activation Random], float4x2[Per-Frame Random]
	if (auto el = _shader.get_parameter("Random"); el != nullptr) {
		if (el.get_type() == streamfx::obs::gs::effect_parameter::type::Matrix) {
			el.set_value(_random_values, 16);
		}
	}

	// int32 RandomSeed: Seed used for random generation
	if (auto el = _shader.get_parameter("RandomSeed"); el != nullptr) {
		if (el.get_type() == streamfx::obs::gs::effect_parameter::type::Integer) {
			el.set_int(_random_seed);
		}
	}

	return;
}

void streamfx::gfx::shader::shader::render(gs_effect* effect)
{
	if (!_shader)
		return;

	if (!effect)
		effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	if (!_rt_up_to_date) {
#ifdef ENABLE_PROFILING
		::streamfx::obs::gs::debug_marker profiler1{::streamfx::obs::gs::debug_color_cache, "Render Cache"};
#endif

		auto op = _rt->render(width(), height());

		gs_ortho(0, 1, 0, 1, 0, 1);
		/*vec4 zero = {0, 0, 0, 0};
		gs_clear(GS_CLEAR_COLOR, &zero, 0, 0);*/

		// Update Blend State
		gs_blend_state_push();
		gs_reset_blend_state();
		gs_enable_blending(false);
		gs_blend_function_separate(GS_BLEND_ONE, GS_BLEND_ZERO, GS_BLEND_ONE, GS_BLEND_ZERO);

		gs_enable_color(true, true, true, true);

		// Fix sRGB Status
		bool old_srgb = gs_framebuffer_srgb_enabled();
		gs_enable_framebuffer_srgb(false);

		while (gs_effect_loop(_shader.get_object(), _shader_tech.c_str())) {
			streamfx::gs_draw_fullscreen_tri();
		}

		// Restore sRGB Status
		gs_enable_framebuffer_srgb(old_srgb);

		// Restore Blend State
		gs_blend_state_pop();

		_rt_up_to_date = true;
	}

	if (auto tex = _rt->get_texture(); tex) {
#ifdef ENABLE_PROFILING
		::streamfx::obs::gs::debug_marker profiler1{::streamfx::obs::gs::debug_color_render, "Draw Cache"};
#endif

		gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), tex->get_object());
		while (gs_effect_loop(effect, "Draw")) {
			gs_draw_sprite(nullptr, 0, width(), height());
		}
	}
}

void streamfx::gfx::shader::shader::set_size(uint32_t w, uint32_t h)
{
	_base_width  = w;
	_base_height = h;
}

void streamfx::gfx::shader::shader::set_input_a(std::shared_ptr<streamfx::obs::gs::texture> tex, bool srgb)
{
	if (!_shader)
		return;

	std::string_view params[] = {
		"InputA",
		"image",
		"tex_a",
	};
	for (auto& name : params) {
		if (streamfx::obs::gs::effect_parameter el = _shader.get_parameter(name.data()); el != nullptr) {
			if (el.get_type() == streamfx::obs::gs::effect_parameter::type::Texture) {
				el.set_texture(tex, srgb);
				break;
			}
		}
	}
}

void streamfx::gfx::shader::shader::set_input_b(std::shared_ptr<streamfx::obs::gs::texture> tex, bool srgb)
{
	if (!_shader)
		return;

	std::string_view params[] = {
		"InputB",
		"image2",
		"tex_b",
	};
	for (auto& name : params) {
		if (streamfx::obs::gs::effect_parameter el = _shader.get_parameter(name.data()); el != nullptr) {
			if (el.get_type() == streamfx::obs::gs::effect_parameter::type::Texture) {
				el.set_texture(tex, srgb);
				break;
			}
		}
	}
}

void streamfx::gfx::shader::shader::set_transition_time(float_t t)
{
	if (!_shader)
		return;

	if (streamfx::obs::gs::effect_parameter el = _shader.get_parameter("TransitionTime"); el != nullptr) {
		if (el.get_type() == streamfx::obs::gs::effect_parameter::type::Float) {
			el.set_float(t);
		}
	}
}

void streamfx::gfx::shader::shader::set_transition_size(uint32_t w, uint32_t h)
{
	if (!_shader)
		return;
	if (streamfx::obs::gs::effect_parameter el = _shader.get_parameter("TransitionSize"); el != nullptr) {
		if (el.get_type() == streamfx::obs::gs::effect_parameter::type::Integer2) {
			el.set_int2(static_cast<int32_t>(w), static_cast<int32_t>(h));
		}
	}
}

void streamfx::gfx::shader::shader::set_visible(bool visible)
{
	_visible = visible;

	for (auto kv : _shader_params) {
		kv.second->visible(visible);
	}
}

void streamfx::gfx::shader::shader::set_active(bool active)
{
	_active = active;

	for (auto kv : _shader_params) {
		kv.second->active(active);
	}

	// Recreate Per-Activation-Random values.
	for (size_t idx = 0; idx < 4; idx++) {
		_random_values[4 + idx] =
			static_cast<float_t>(static_cast<double_t>(_random()) / static_cast<double_t>(_random.max()));
	}
}

obs_source_t* streamfx::gfx::shader::shader::get()
{
	return _self;
}

std::filesystem::path streamfx::gfx::shader::shader::get_shader_file()
{
	return _shader_file;
}
