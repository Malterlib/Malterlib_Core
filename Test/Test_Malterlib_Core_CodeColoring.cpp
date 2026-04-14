// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

// Comment
// http://example.com

/// Doc comment
/// ===========
/// @brief \brief
/// http://example.com

/**
	Doc comment
	===========
	@brief \brief
	http://example.com
*/

#define DMacro(d_MacroParameter) \
	d_MacroParameter

auto g_String = "String";
auto g_Char = 'C';
[[maybe_unused]] const static uint32 gc_GlobalConstant = (55 + 5 * 6 % ((67 | 77) & 88));
[[maybe_unused]] static int gs_ThousandsSeparator = 10'000'000;
double g_Double = 5.655 + 7.66e10;

template <typename t_CType>
concept cComparable = true;

namespace NTest
{
	template <typename tf_CType, int tf_NonType, int ...tfp_Values>
	void inline fg_FunctionGlobal();

	enum ETest
	{
		ETest_Value
	};

	template <typename t_CType, int t_NonType>
	struct TCType
	{
		void f_FunctionPublic
		(
			uint32 _FunctionParameter
			, uint32 &o_OutputFunctionParameter
			, NMib::NFunction::TCFunction<void ()> const &_fFunctor
			, NMib::NFunction::TCFunction<void ()> &o_fFunctor
		) const volatile
		{
			[[maybe_unused]] ETest EnumValue = ETest_Value;

			for (;;)
			{
			}

			[[maybe_unused]] auto pAutoVar = nullptr;

			auto fFunctor = []
				{
				}
			;

			fFunctor();
			o_fFunctor();
			const_cast<TCType const *>(this)->m_fFunctor();
			const_cast<TCType const *>(this)->mp_fFunctor();

			fFunctor.f_Clear();
		}

		NMib::NFunction::TCFunction<void ()> m_fFunctor;

		uint32 m_VariablePublic;
		static uint32 ms_StaticVariablePublic;
		constexpr static uint32 mc_ConstantPublic = 0;

	private:
		NMib::NFunction::TCFunction<void ()> mp_fFunctor;

		uint32 mp_VariablePrivate;
		static uint32 msp_StaticVariablePrivate;
		constexpr static uint32 mcp_ConstantPrivate = 0;

		void fp_FunctionPrivate();

	};
}

#if 0

// Language
= * + - % {} [] () <> // #ffffff
# // #ffffff
for if while do // #ffffff
typename // #c0c0c0
inline // #c0c0c0
\ // #808080
public private protected friend // #ffc8ca
const volatile // #ffb680

// Builtin types
int bool void uint32 // #ff5966

// Constant values
15 60.6 // #ff0080
t_Test // #ff5bad
gc_Test ETest_Value mc_Test gc_Test c_Test true false nullptr // #ff8ac5
mcp_Test // #ca97b1
tf_Test // #ffb7db

// Character
'T' // #ff48f0

// Namespace
NTest // #d785ff

// Types
t_CTest t_TCTest // #8269ff
tf_CTest tf_TCTest // #cdc3ff
CTest TCTest ETest // #b8aaff
auto // #dbd3ff

// String
"String" // #009eff

// Functors
_fTest // #00e4e6
o_fTest // #36e8cd
fTest // #00edae
m_fTest // #00f265
mp_fTest // #4fc17e

// Functions
f_Test fs_Test // #26ff00
fg_Test fsg_Test // #1cb900
fp_Test fsp_Test  // #8dd580

// Parameters
_Test p_Test // #e6ff00
o_Test po_Test // #fff54b

// Variables
Var // #ffd700

// Concepts
cComparable // #ffb680

// Member variables
m_Test // #ffa600
mp_Test // #c59d53

// Macros
DTest // #ff7700
d_Test // #ffbc81

// Globals
ms_Test // #ff3f1c
g_Test gs_Test // #e13819
msp_Test // #d56955

// Comments
// Comment #003737 #ff9f3b
/// Documentation comment #003737 #9bcde6
/// @Keyword #003737 #f070b6
/// http://example.com #003737 #9dff70

#endif
