// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#ifdef __clang_analyzer__
template <typename tf_CObject>
void fg_Unconsume(tf_CObject &o_Value)
{
	fg_UnconsumeUndefined(&o_Value);
}
#endif
