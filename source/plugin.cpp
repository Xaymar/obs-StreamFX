// AUTOGENERATED COPYRIGHT HEADER START
// Copyright (C) 2017-2023 Michael Fabian 'Xaymar' Dirks <info@xaymar.com>
// AUTOGENERATED COPYRIGHT HEADER END

#include "plugin.hpp"
#include "configuration.hpp"
#include "gfx/gfx-opengl.hpp"
#include "obs/gs/gs-helper.hpp"
#include "obs/gs/gs-vertexbuffer.hpp"

#ifdef ENABLE_NVIDIA_CUDA
#include "nvidia/cuda/nvidia-cuda-obs.hpp"
#endif

#ifdef ENABLE_ENCODER_AOM_AV1
#include "encoders/encoder-aom-av1.hpp"
#endif

#ifdef ENABLE_FILTER_DISPLACEMENT
#include "filters/filter-displacement.hpp"
#endif

#ifdef ENABLE_FRONTEND
#include "ui/ui.hpp"
#endif

#ifdef ENABLE_UPDATER
#include "updater.hpp"
//static std::shared_ptr<streamfx::updater> _updater;
#endif

#include "warning-disable.hpp"
#include <fstream>
#include <list>
#include <map>
#include <stdexcept>
#include "warning-enable.hpp"

static std::shared_ptr<streamfx::gfx::opengl> _streamfx_gfx_opengl;

namespace streamfx {
	typedef std::list<loader_function_t>               loader_list_t;
	typedef std::map<loader_priority_t, loader_list_t> loader_map_t;

	loader_map_t& get_initializers()
	{
		static loader_map_t initializers;
		return initializers;
	}

	loader_map_t& get_finalizers()
	{
		static loader_map_t finalizers;
		return finalizers;
	}

	loader::loader(loader_function_t initializer, loader_function_t finalizer, loader_priority_t priority)
	{
		auto init_kv = get_initializers().find(priority);
		if (init_kv != get_initializers().end()) {
			init_kv->second.push_back(initializer);
		} else {
			get_initializers().emplace(priority, loader_list_t{initializer});
		}

		// Invert the order for finalizers.
		auto ipriority = priority ^ static_cast<loader_priority_t>(0xFFFFFFFFFFFFFFFF);
		auto fina_kv   = get_finalizers().find(ipriority);
		if (fina_kv != get_finalizers().end()) {
			fina_kv->second.push_back(finalizer);
		} else {
			get_finalizers().emplace(ipriority, loader_list_t{finalizer});
		}
	}
} // namespace streamfx

MODULE_EXPORT bool obs_module_load(void)
{
	try {
		DLOG_INFO("Loading Version %s", STREAMFX_VERSION_STRING);

		// Run all initializers.
		for (auto kv : streamfx::get_initializers()) {
			for (auto init : kv.second) {
				try {
					init();
				} catch (const std::exception& ex) {
					DLOG_ERROR("Initializer threw exception: %s", ex.what());
				} catch (...) {
					DLOG_ERROR("Initializer threw unknown exception.");
				}
			}
		}

		// Initialize GLAD (OpenGL)
		{
			streamfx::obs::gs::context gctx{};
			if (gs_get_device_type() == GS_DEVICE_OPENGL) {
				_streamfx_gfx_opengl = streamfx::gfx::opengl::get();
			}
		}

#ifdef ENABLE_NVIDIA_CUDA
		// Initialize CUDA if features requested it.
		std::shared_ptr<::streamfx::nvidia::cuda::obs> cuda;
		try {
			cuda = ::streamfx::nvidia::cuda::obs::get();
		} catch (...) {
			// If CUDA failed to load, it is considered safe to ignore.
		}
#endif

		// Encoders
		{
#ifdef ENABLE_ENCODER_AOM_AV1
			streamfx::encoder::aom::av1::aom_av1_factory::initialize();
#endif
		}

		// Filters
		{
#ifdef ENABLE_FILTER_DISPLACEMENT
			streamfx::filter::displacement::displacement_factory::initialize();
#endif
		}

		DLOG_INFO("Loaded Version %s", STREAMFX_VERSION_STRING);
		return true;
	} catch (std::exception const& ex) {
		DLOG_ERROR("Unexpected exception in function '%s': %s", __FUNCTION_NAME__, ex.what());
		return false;
	} catch (...) {
		DLOG_ERROR("Unexpected exception in function '%s'.", __FUNCTION_NAME__);
		return false;
	}
}

MODULE_EXPORT void obs_module_unload(void)
{
	try {
		DLOG_INFO("Unloading Version %s", STREAMFX_VERSION_STRING);

		// Filters
		{
#ifdef ENABLE_FILTER_DISPLACEMENT
			streamfx::filter::displacement::displacement_factory::finalize();
#endif
		}

		// Encoders
		{
#ifdef ENABLE_ENCODER_AOM_AV1
			streamfx::encoder::aom::av1::aom_av1_factory::finalize();
#endif
		}

		// Finalize GLAD (OpenGL)
		{
			streamfx::obs::gs::context gctx{};
			_streamfx_gfx_opengl.reset();
		}

		//	// Auto-Updater
		//#ifdef ENABLE_UPDATER
		//	_updater.reset();
		//#endif

		// Run all finalizers.
		for (auto kv : streamfx::get_finalizers()) {
			for (auto init : kv.second) {
				try {
					init();
				} catch (const std::exception& ex) {
					DLOG_ERROR("Finalizer threw exception: %s", ex.what());
				} catch (...) {
					DLOG_ERROR("Finalizer threw unknown exception.");
				}
			}
		}

		DLOG_INFO("Unloaded Version %s", STREAMFX_VERSION_STRING);
	} catch (std::exception const& ex) {
		DLOG_ERROR("Unexpected exception in function '%s': %s", __FUNCTION_NAME__, ex.what());
	} catch (...) {
		DLOG_ERROR("Unexpected exception in function '%s'.", __FUNCTION_NAME__);
	}
}

std::shared_ptr<streamfx::util::threadpool::threadpool> streamfx::threadpool()
{
	return streamfx::util::threadpool::threadpool::instance();
}

std::filesystem::path streamfx::data_file_path(std::string_view file)
{
	const char* root_path = obs_get_module_data_path(obs_current_module());
	if (root_path) {
		auto ret = std::filesystem::u8path(root_path);
		ret.append(file.data());
		return ret;
	} else {
		throw std::runtime_error("obs_get_module_data_path returned nullptr");
	}
}

std::filesystem::path streamfx::config_file_path(std::string_view file)
{
	char* root_path = obs_module_get_config_path(obs_current_module(), file.data());
	if (root_path) {
		auto ret = std::filesystem::u8path(root_path);
		bfree(root_path);
		return ret;
	} else {
		throw std::runtime_error("obs_module_get_config_path returned nullptr");
	}
}

#ifdef ENABLE_FRONTEND
bool streamfx::open_url(std::string_view url)
{
	QUrl qurl = QString::fromUtf8(url.data());
	return QDesktopServices::openUrl(qurl);
}
#endif
