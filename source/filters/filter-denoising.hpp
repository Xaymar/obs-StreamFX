// AUTOGENERATED COPYRIGHT HEADER START
// Copyright (C) 2021-2023 Michael Fabian 'Xaymar' Dirks <info@xaymar.com>
// AUTOGENERATED COPYRIGHT HEADER END

#pragma once
#include "obs/gs/gs-effect.hpp"
#include "obs/gs/gs-rendertarget.hpp"
#include "obs/gs/gs-texture.hpp"
#include "obs/obs-source-factory.hpp"
#include "plugin.hpp"
#include "util/util-threadpool.hpp"

#include "warning-disable.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include "warning-enable.hpp"

#ifdef ENABLE_FILTER_DENOISING_NVIDIA
#include "nvidia/vfx/nvidia-vfx-denoising.hpp"
#endif

namespace streamfx::filter::denoising {
	enum denoising_provider {
		INVALID          = -1,
		AUTOMATIC        = 0,
		NVIDIA_DENOISING = 1,
	};

	const char* cstring(denoising_provider provider);

	std::string string(denoising_provider provider);

	class denoising_instance : public obs::source_instance {
		std::pair<uint32_t, uint32_t> _size;

		denoising_provider                      _provider;
		denoising_provider                      _provider_ui;
		std::atomic<bool>                       _provider_ready;
		std::mutex                              _provider_lock;
		std::shared_ptr<util::threadpool::task> _provider_task;

		std::shared_ptr<::streamfx::obs::gs::effect>  _standard_effect;
		std::shared_ptr<::streamfx::obs::gs::sampler> _channel0_sampler;
		std::shared_ptr<::streamfx::obs::gs::sampler> _channel1_sampler;

		std::shared_ptr<::streamfx::obs::gs::rendertarget> _input;
		std::shared_ptr<::streamfx::obs::gs::texture>      _output;
		bool                                               _dirty;

#ifdef ENABLE_FILTER_DENOISING_NVIDIA
		std::shared_ptr<::streamfx::nvidia::vfx::denoising> _nvidia_fx;
#endif

		public:
		denoising_instance(obs_data_t* data, obs_source_t* self);
		~denoising_instance() override;

		void load(obs_data_t* data) override;
		void migrate(obs_data_t* data, uint64_t version) override;
		void update(obs_data_t* data) override;
		void properties(obs_properties_t* properties);

		uint32_t get_width() override;
		uint32_t get_height() override;

		void video_tick(float time) override;
		void video_render(gs_effect_t* effect) override;

		private:
		void switch_provider(denoising_provider provider);
		void task_switch_provider(util::threadpool::task_data_t data);

#ifdef ENABLE_FILTER_DENOISING_NVIDIA
		void nvvfx_denoising_load();
		void nvvfx_denoising_unload();
		void nvvfx_denoising_size();
		void nvvfx_denoising_process();
		void nvvfx_denoising_properties(obs_properties_t* props);
		void nvvfx_denoising_update(obs_data_t* data);
#endif
	};

	class denoising_factory : public obs::source_factory<::streamfx::filter::denoising::denoising_factory, ::streamfx::filter::denoising::denoising_instance> {
#ifdef ENABLE_FILTER_DENOISING_NVIDIA
		bool                                           _nvidia_available;
		std::shared_ptr<::streamfx::nvidia::cuda::obs> _nvcuda;
		std::shared_ptr<::streamfx::nvidia::cv::cv>    _nvcvi;
		std::shared_ptr<::streamfx::nvidia::vfx::vfx>  _nvvfx;
#endif

		public:
		virtual ~denoising_factory();
		denoising_factory();

		virtual const char* get_name() override;

		virtual void              get_defaults2(obs_data_t* data) override;
		virtual obs_properties_t* get_properties2(denoising_instance* data) override;

#ifdef ENABLE_FRONTEND
		static bool on_manual_open(obs_properties_t* props, obs_property_t* property, void* data);
#endif

		bool               is_provider_available(denoising_provider);
		denoising_provider find_ideal_provider();

		public: // Singleton
		static void                                                              initialize();
		static void                                                              finalize();
		static std::shared_ptr<::streamfx::filter::denoising::denoising_factory> instance();
	};

} // namespace streamfx::filter::denoising
