// AUTOGENERATED COPYRIGHT HEADER START
// Copyright (C) 2020-2023 Michael Fabian 'Xaymar' Dirks <info@xaymar.com>
// AUTOGENERATED COPYRIGHT HEADER END

#include "av1.hpp"

const char* streamfx::encoder::codec::av1::profile_to_string(profile p)
{
	switch (p) {
	case profile::MAIN:
		return D_TRANSLATE(S_CODEC_AV1_PROFILE ".Main");
	case profile::HIGH:
		return D_TRANSLATE(S_CODEC_AV1_PROFILE ".High");
	case profile::PROFESSIONAL:
		return D_TRANSLATE(S_CODEC_AV1_PROFILE ".Professional");
	default:
		return "Unknown";
	}
}
