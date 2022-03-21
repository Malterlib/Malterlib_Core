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

	// Concept
	cComparable

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
