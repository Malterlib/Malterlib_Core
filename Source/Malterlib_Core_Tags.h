// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib
{
	template <typename ...tfp_CTag>
	struct TCTags;

	template <typename t_CTags, typename t_CBaseTag, typename t_CDefaultTag = t_CBaseTag>
	struct TCGetTag;

	template <typename t_CTags, typename t_CBaseTag>
	struct TCHasTag;

	template <typename t_CTags, typename ...tp_CTagsToRemove>
	struct TCRemoveTags;

	template <typename t_CTags, typename ...tp_CTagsToAdd>
	struct TCAddTags;

}

#include "Malterlib_Core_Tags.hpp"
