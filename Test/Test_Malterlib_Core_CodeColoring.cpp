// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#	include <Mib/Core/Core>

// Comment
// http://example.com

///
/// Doc comment
/// ===========
/// @brief \brief
/// http://example.com

#define DMacro(d_MacroParameter) \
	d_MacroParameter

auto g_String = "String";
auto g_Char = 'C';
[[maybe_unused]] const static uint32 gc_GlobalConstant = (55 + 5 * 6 % ((67 | 77) & 88));
[[maybe_unused]] static int gs_ThousandsSeparator = 10'000'000;
double g_Double = 5.655 + 7.66e10;

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
		uint32 ms_StaticVariablePublic;
		uint32 mc_ConstantPublic;
		
	private:
		NMib::NFunction::TCFunction<void ()> mp_fFunctor;

		uint32 mp_VariablePrivate;
		static uint32 msp_StaticVariablePrivate;
		static const uint32 mcp_ConstantPrivate;

		void fp_FunctionPrivate();
		
	};
}
