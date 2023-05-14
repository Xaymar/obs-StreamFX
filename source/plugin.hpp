// AUTOGENERATED COPYRIGHT HEADER START
// Copyright (C) 2019-2023 Michael Fabian 'Xaymar' Dirks <info@xaymar.com>
// AUTOGENERATED COPYRIGHT HEADER END

#pragma once
#include "common.hpp"

#include "warning-disable.hpp"
#include <cstdint>
#include <functional>
#include "warning-enable.hpp"

namespace streamfx {
	/** Simple but efficient loader structure for ordered initia-/finalize.
	 * 
	 */
	typedef int32_t               loader_priority_t;
	typedef std::function<void()> loader_function_t;
	enum loader_priority : loader_priority_t {
		HIGHEST = INT32_MIN,
		HIGHER  = INT32_MIN / 4 * 3,
		HIGH    = INT32_MIN / 4 * 2,
		ABOVE   = INT32_MIN / 4,
		NORMAL  = 0,
		BELOW   = INT32_MAX / 4,
		LOW     = INT32_MAX / 4 * 2,
		LOWER   = INT32_MAX / 4 * 3,
		LOWEST  = INT32_MAX,
	};

	struct loader {
		loader(loader_function_t initializer, loader_function_t finalizer, loader_priority_t priority);

		// Usage:
		// auto loader = streamfx::loader([]() { ... }, []() { ... }, 0);
	};

	// Threadpool
	std::shared_ptr<streamfx::util::threadpool::threadpool> threadpool();

	std::filesystem::path data_file_path(std::string_view file);
	std::filesystem::path config_file_path(std::string_view file);

#ifdef ENABLE_FRONTEND
	bool open_url(std::string_view url);
#endif
} // namespace streamfx
