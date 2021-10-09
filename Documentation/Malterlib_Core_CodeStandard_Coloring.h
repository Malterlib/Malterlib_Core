// Use in editor to test coloring

#if 0
	#include <Mib/Core/Core>

	// Comment
	// http://example.com


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
				, uint32 &o_OutputFunctionParameter
				, NMib::NFunction::TCFunction<void ()> _fFunctor
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
				_fFunctor();
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
			static uint32 const mcp_ConstantPrivate;

			void fp_FunctionPrivate();

		};
	}

	// Language
	= * + - % {} [] () <>
	#
	for if while do
	typename
	inline
	\
	public private protected friend
	const volatile

	// Builtin types
	int bool void uint32

	// Constant values
	15 60.6
	t_Test
	gc_Test ETest_Value mc_Test gc_Test c_Test true false nullptr
	mcp_Test
	tf_Test

	// Character
	'T'

	// Namespace
	NTest

	// Types
	t_CTest t_TCTest
	tf_CTest tf_TCTest
	CTest TCTest ETest
	auto

	// String
	"String"

	// Functors
	_fTest
	o_fTest
	fTest
	m_fTest
	mp_fTest

	// Functions
	f_Test fs_Test
	fg_Test fsg_Test
	fp_Test fsp_Test

	// Parameters
	_Test p_Test
	o_Test po_Test

	// Variables
	Var

	// Member variables
	m_Test
	mp_Test

	// Macros
	DTest
	d_Test

	// Globals
	ms_Test
	g_Test gs_Test
	msp_Test

#endif
