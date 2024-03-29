// AUTOGENERATED COPYRIGHT HEADER START
// Copyright (C) 2017-2023 Michael Fabian 'Xaymar' Dirks <info@xaymar.com>
// Copyright (C) 2022 lainon <GermanAizek@yandex.ru>
// AUTOGENERATED COPYRIGHT HEADER END

#pragma once
#include "common.hpp"
#include "gs-effect-parameter.hpp"
#include "gs-effect-technique.hpp"

#include "warning-disable.hpp"
#include <filesystem>
#include <list>
#include "warning-enable.hpp"

namespace streamfx::obs::gs {
	class effect : public std::shared_ptr<gs_effect_t> {
		public:
		effect() = default;
		effect(std::string_view code, std::string_view name);
		effect(std::filesystem::path file);
		~effect();

		std::size_t                         count_techniques();
		streamfx::obs::gs::effect_technique get_technique(std::size_t idx);
		streamfx::obs::gs::effect_technique get_technique(std::string_view name);
		bool                                has_technique(std::string_view name);

		std::size_t                         count_parameters();
		streamfx::obs::gs::effect_parameter get_parameter(std::size_t idx);
		streamfx::obs::gs::effect_parameter get_parameter(std::string_view name);
		bool                                has_parameter(std::string_view name);
		bool                                has_parameter(std::string_view name, effect_parameter::type type);

		public /* Legacy Support */:
		inline gs_effect_t* get_object()
		{
			return get();
		}

		static streamfx::obs::gs::effect create(std::string_view code, std::string_view name)
		{
			return streamfx::obs::gs::effect(code, name);
		};

		static streamfx::obs::gs::effect create(std::string_view file)
		{
			return streamfx::obs::gs::effect(std::filesystem::path(file));
		};

		static streamfx::obs::gs::effect create(const std::filesystem::path& file)
		{
			return streamfx::obs::gs::effect(file);
		};
	};
} // namespace streamfx::obs::gs
