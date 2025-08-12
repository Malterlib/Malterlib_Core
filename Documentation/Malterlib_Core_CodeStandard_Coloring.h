// Use in editor to test coloring

#if 0
	#include <Mib/Core/Core>
	#	include   <Mib/Core/Core>
	#include /* Testing */ <Mib/Core/Core>

	#include "Mib/Core/Core"
	#	include 	"Mib/Core/Core"
	#include /* Testing */ "Mib/Core/Core"

	// Comment
	// http://example.com

	/// Doc comment
	/// ===========
	/// @brief \brief
	/// http://example.com

	#define DMacro(d_MacroParameter) \
		d_MacroParameter

	using uint32 = unsigned int;

	char const *g_String = "String";
	auto g_Char = 'C';
	[[maybe_unused]] constexpr static uint32 gc_GlobalConstant = (55 + 5 * 6 % ((67 | 77) & 88));
	[[maybe_unused]] static int gs_ThousandsSeparator = 10'000'000;
	double g_Double = 5.655 + 7.66e10;
	constexpr double gc_Double2 = 5.655 + 7.66e10;

	template <typename t_CType>
	concept cComparable = true;

	namespace NTest
	{
		template <typename tf_CType, int tf_NonType, int ...tfp_Values>
		void inline fg_FunctionGlobal()
		{
			g_String = "String2";
		}

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

				DMacro(1);

				fFunctor();
				o_fFunctor();
				const_cast<TCType const *>(this)->m_fFunctor();
				const_cast<TCType const *>(this)->mp_fFunctor();

				fFunctor.f_Clear();

				uint32 Variable = 0;
				static uint32 s_StaticVariable = 0;
				static uint32 const s_StaticVariableConst = 0;
				constexpr static uint32 c_Constant = 0;

				bool bComparable = cComparable<t_CType>;

				int TestValue =
					m_VariablePublic
					+ ms_StaticVariablePublic
					+ ms_StaticVariablePublicConst
					+ mc_ConstantPublic
					+ mp_VariablePrivate
					+ msp_StaticVariablePrivate
					+ msp_StaticVariablePrivateConst
					+ mcp_ConstantPrivate
					+ Variable
					+ s_StaticVariable
					+ s_StaticVariableConst
					+ c_Constant
				;
			}

			NMib::NFunction::TCFunction<void ()> m_fFunctor;

			uint32 m_VariablePublic;
			static uint32 ms_StaticVariablePublic;
			static uint32 const ms_StaticVariablePublicConst;
			constexpr static uint32 mc_ConstantPublic = 0;

		private:
			NMib::NFunction::TCFunction<void ()> mp_fFunctor;

			uint32 mp_VariablePrivate;
			static uint32 msp_StaticVariablePrivate;
			static uint32 const msp_StaticVariablePrivateConst;
			static constexpr uint32 mcp_ConstantPrivate = 0;

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
