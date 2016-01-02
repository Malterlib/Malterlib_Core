// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

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
const static uint32 gc_GlobalConstant = 55;

namespace NTest
{
	template <typename tf_CType, int tf_NonType>
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
			, uint32 & o_OutputFunctionParameter
			, NMib::NFunction::TCFunction<void ()> const &_fFunctor
			, NMib::NFunction::TCFunction<void ()> &o_fFunctor
		) const volatile
		{
			ETest EnumValue = ETest_Value;
			
			for(;;)
			{
			}
			
			auto pAutoVar = nullptr;
			
			auto fFunctor = []
				{
				}
			;

			fFunctor();
			o_fFunctor();
			m_fFunctor();
			mp_fFunctor();
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
