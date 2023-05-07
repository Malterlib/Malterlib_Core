// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>


template <typename ...tf_CType>
void fg_Test(tf_CType ...p_bTest)
{
	[[maybe_unused]] static auto s_fTest = []{};
}
void fg_Test2()
{
	[[maybe_unused]] static auto s_fTest = []{};
}

auto g_Test2 = &fg_Test2;

#if 0

void fg_TestOut(int*& _opTest)
{
	auto pTest = nullptr;
	if (pTest)
		_opTest = pTest;
	
	m_PendingAtomic;
}

void fg_TestOut(int*& o_pTest)
{
	auto pTest = nullptr;
	if (pTest)
		o_pTest = pTest;
}

void fg_TestOut(TCSet<int>& _ocTest)
{
	_ocTest[55];
	_ocTest[66];
}

void fg_TestOut(TCSet<int>& o_cTest)
{
	o_cTest[55];
	o_cTest[66];
}

void fg_TestOut(TCSet<int>& _oTest)
{
	_oTest[55];
	_oTest[66];
}

void fg_TestOut(bool& _obTest)
{
	if (x)
		_obTest = true;
}

void fg_TestOut(bool& o_bTest)
{
	if (x)
		o_bTest = true;
}

void fg_TestOut(bool& _oTest)
{
	if (x)
		_oTest = true;
}



void (int& _oTest)
void (bool& _obTest)
void (TCSet<int>& _ocTest)
void (mint& _oiTest)
void (mint& _onTest)
void (int*& _opTest)
void (FTest& _ofTest)
void f_Func(t_CInt& ...p_oTest)
void f_Func(t_CBool& ...p_obTest)
void f_Func(t_CContainer& ...p_ocTest)
void f_Func(t_CIndex& ...p_oiTest)
void f_Func(t_CNum& ...p_onTest)
void f_Func(t_CPointer& ...p_opTest)
void f_Func(t_FFunc& ...p_ofTest)
#endif

#if 0


template <uint32 t_Test>
template <typename t_CTest>
template <typename t_FTest>
template <template <typename> class t_TCTest>

template <uint32 ...tp_Test>
template <typename ...tp_CTest>
template <typename ...tp_FTest>
template <template <typename> class ...tp_TCTest>

template <uint32 tf_Test>
template <typename tf_CTest>
template <typename tf_FTest>
template <template <typename> class tf_TCTest>

template <uint32 ...tfp_Test>
template <typename ...tfp_CTest>
template <typename ...tfp_FTest>
template <template <typename> class ...tfp_TCTest>

enum ETest
{
	ETest_Value = 0
};

namespace NTest
{
}

uint32 m_Category_Var1;
uint32 m_Category_Var2;
uint32 m_Category_Var3;
uint32 m_Category_Var4;
uint32 m_Category_Var5;

struct CTest
{
};
class CTest
{
};
typedef int CTestInt;

struct ICTest
{ 
	virtual void f_Function() = 0; 
};

typedef void (FTest)();
FTest* fTest = []{};

template <typename t_CType>
struct TCTest
{
};

template <typename t_CType>
struct TICTest
{
	virtual void f_Function() = 0; 
};


void (int _Test)
void (bool _bTest)
void (TCSet<int> _cTest)
void (mint _iTest)
void (mint _nTest)
void (int* _pTest)
void (FTest _fTest)

void f_Func(t_CInt ...p_Test)
void f_Func(t_CBool ...p_bTest)
void f_Func(t_CContainer ...p_cTest)
void f_Func(t_CIndex ...p_iTest)
void f_Func(t_CNum ...p_nTest)
void f_Func(t_CPointer ...p_pTest)
void f_Func(t_FFunc ...p_fTest)

void (int& o_Test)
void (bool& o_bTest)
void (TCSet<int>& o_cTest)
void (mint& o_iTest)
void (mint& o_nTest)
void (int*& o_pTest)
void (FTest& o_fTest)


void f_Func(t_CInt& ...po_Test)
void f_Func(t_CBool& ...po_bTest)
void f_Func(t_CContainer& ...po_cTest)
void f_Func(t_CIndex& ...po_iTest)
void f_Func(t_CNum& ...po_nTest)
void f_Func(t_CPointer& ...po_pTest)
void f_Func(t_FFunc& ...po_fTest)

#if 0 // Deprecated?
fr_Test
fsr_Test
fpr_Test
fspr_Test
fgr_Test
fsgr_Test
#endif

void f_Test()
void f_rTest()

void fp_Test()
void fp_rTest()

static void fs_Test()
static void fs_rTest()

static void fsp_Test()
static void fsp_rTest()

void fg_Test()
void fg_rTest()

static void fsg_Test()
static void fsg_rTest()



#define DTest

#define DTest(d_Test)
#define DTest(d_bTest)
#define DTest(d_cTest)
#define DTest(d_iTest)
#define DTest(d_nTest)
#define DTest(d_pTest)
#define DTest(d_fTest)
#define DTest(d_NTest)
#define DTest(d_CTest)
#define DTest(d_FTest)
#define DTest(d_ICTest)
#define DTest(d_TCTest)
#define DTest(d_TICTest)

// bcinpf

inline_always
inline_never
module_import
assure_used
align_cacheline
likely
unlikely
pure
uint32
fp64
mint

bool bTest = true;
void (bool _bTest);
struct CTest
{
	bool m_bTest;
	uint32 m_bTest:1;
	uint32 m_bTest2:2;
};

TCSet<uint32> Indices;
TCVector<uint32> Indices;

CByteVector ByteBuffer;
auto rByte = fg_Range(ByteBuffer);
for (auto iByte = rByte.f_Front(); iByte != rByte.f_Back(); ++iByte)
	*iByte;

for (mint iByte = 0; iByte < ByteBuffer.f_GetLen(); ++iByte)
	ByteBuffer[iByte];

CByteVector ByteBuffer;
for (auto rByte = fg_Range(ByteBuffer); rByte; ++rByte)
	*rByte;

CByteVector ByteBuffer;
mint nBytes = ByteBuffer.f_GetLen();

CByteVector ByteBuffer;
uint8 const *pBytes = ByteBuffer.f_GetArray();


typedef void (FTest)(int);
class CTest
{
	TCFunction<void ()> m_fTest;
	FTest *m_fTest2;
};
void fg_Test(CTest const &_Test)
{
	auto fTest = []()
		{
			_Test.m_fTest();
			if (_Test.m_fTest2)
				_Test.m_fTest2();
		}
	;
	fTest();
}

void fg_rTest(uint32 _Value)
{
	if (_Value > 0)
		fg_rTest(_Value - 1)
}



uint32 Test = 0;
bool bTest = true;
TCSet<int> cTest;
mint iTest = 0;
mint nTest = 0;
int* pTest = nullptr;
FTest* fTest = &fg_Test;
auto fTest = []{};

static const uint32 c_Test = 0;
static const bool c_bTest = true;
static const mint c_iTest = 0;
static const mint c_nTest = 0;

static uint32 s_Test = 0;
static bool s_bTest = true;
static TCSet<int> s_cTest;
static mint s_iTest = 0;
static mint s_nTest = 0;
static int* s_pTest = nullptr;
static FTest* s_fTest = &fg_Test;
static auto s_fTest = []{};


uint32 g_Test = 0;
bool g_bTest = true;
TCSet<int> g_cTest;
mint g_iTest = 0;
mint g_nTest = 0;
int* g_pTest = nullptr;
FTest* g_fTest = &fg_Test;
auto g_fTest = []{};

static const uint32 gc_Test = 0;
static const bool gc_bTest = true;
static const mint gc_iTest = 0;
static const mint gc_nTest = 0;

static uint32 gs_Test = 0;
static bool gs_bTest = true;
static TCSet<int> gs_cTest;
static mint gs_iTest = 0;
static mint gs_nTest = 0;
static int* gs_pTest = nullptr;
static FTest* gs_fTest = &fg_Test;
static auto gs_fTest = []{};


class CTest
{
	uint32 m_Test;
	bool m_bTest;
	TCSet<int> m_cTest;
	mint m_iTest;
	mint m_nTest;
	int* m_pTest;
	FTest* m_fTest;
	TCFunction<void ()> m_fTest;
};

class CTest
{
	static const uint32 mc_Test = 0;
	static const bool mc_bTest = true;
	static const mint mc_iTest = 0;
	static const mint mc_nTest = 0;
};


class CTest
{
	static uint32 ms_Test;
	static bool ms_bTest;
	static TCSet<int> ms_cTest;
	static mint ms_iTest;
	static mint ms_nTest;
	static int* ms_pTest;
	static FTest* ms_fTest;
	static auto ms_fTest;
};


class CTest
{
	uint32 mp_Test;
	bool mp_bTest;
	TCSet<int> mp_cTest;
	mint mp_iTest;
	mint mp_nTest;
	int* mp_pTest;
	FTest* mp_fTest;
	TCFunction<void ()> mp_fTest;
};

class CTest
{
	static const uint32 mcp_Test = 0;
	static const bool mcp_bTest = true;
	static const mint mcp_iTest = 0;
	static const mint mcp_nTest = 0;
};


class CTest
{
	static uint32 msp_Test;
	static bool msp_bTest;
	static TCSet<int> msp_cTest;
	static mint msp_iTest;
	static mint msp_nTest;
	static int* msp_pTest;
	static FTest* msp_fTest;
	static auto msp_fTest;
};



#endif

// Types

// pType:
// C
struct CTest;

// IC
struct ICTest
{
	virtual void f_Test() = 0;
};

// PF (Deprecated)

// F
typedef void (FTest)(int);

// pTemplateType:
// TC
template <typename>
struct TCTest;

// TIC
template <typename>
struct TICTest2
{
	virtual void f_Test() = 0;
};

// pNamespace:
// N
namespace NTest
{
}

// pEnum, pEnumerator:
// E
enum ETest
{
	ETest_Enumerator
};



// pGlobalVariable:
// g_
uint32 g_Test;

// pGlobalStaticVariable:
// gs_
[[maybe_unused]] static uint32 gs_Test;

// pGlobalConstant:
// gc_
[[maybe_unused]] static const uint32 gc_Test = 0;
 

// Functions

// pStaticFunction:
// fsg_
[[maybe_unused]] static void fsg_Test()
{
}

// fsgr_
[[maybe_unused]] static void fsgr_Test()
{
	if (g_Test)
		return fsgr_Test();
}

// pFunction:
// fg_
void fg_Test()
{
}

// fgr_
void fgr_Test()
{
	if (g_Test)
		return fgr_Test();
}


class CTest2
{
public:
	// pMemberFunctionPublic:
	// f_
	void f_Test()
	{
	}
	// fr_
	void fr_Test()
	{
		if (g_Test)
			return fr_Test();
	}
	
	// pMemberStaticFunctionPublic
	// fs_
	static void fs_Test()
	{
	}
	
	// fsr_
	static void fsr_Test()
	{
		if (g_Test)
			return fsr_Test();
	}

	// pMemberVariablePublic:
	// m_
	uint32 m_Test;

	// pMemberStaticVariablePublic:
	// ms_
	static uint32 ms_Test;

	// pMemberConstantPublic:
	// mc_
	static const uint32 mc_Test = 0;
	
private:

	// pMemberFunctionPrivate:
	// fp_
	void fp_Test()
	{
	}
	
	// fpr_
	void fpr_Test()
	{
		if (g_Test)
			return fpr_Test();
	}
	
	// pMemberStaticFunctionPrivate
	// fsp_
	static void fsp_Test()
	{
	}
	
	// fspr_
	static void fspr_Test()
	{
		if (g_Test)
			return fspr_Test();
	}

	// pMemberVariablePrivate:
	// mp_
	uint32 mp_Test;

	// pMemberStaticVariablePrivate:
	// msp_
	static uint32 msp_Test;

	// pMemberConstantPrivate:
	// mcp_
	static const uint32 mcp_Test = 0;
	
};
	
// Params

// pFunctionParameter:
// _
void fg_Test(uint32 _Test)
{
}

// pFunctionParameter_Output:
// _o
// o_
void fg_Test(uint32 &o_Test)
{
	o_Test = 5;
}

// pFunctionParameter_ParamPack:
// p_
template <typename ...tfp_CParams>
void fg_Test(tfp_CParams ...p_Test);

// pFunctionParameter_OutputParamPack:
// po_
template <typename ...tfp_CParams>
void fg_Test(tfp_CParams & ...p_Test);

// Macros

// pMacroParameter:
// d_
#define DTest(d_Param) d_Param

// pMacro:
// D
#define DTest2



// pTemplateNonTypeParam:
// t_
template <int t_Test>
struct TCTest3;

// tp_
template <int ...tp_Test>
struct TCTest4;

// pTemplateTypeParam:
// tp_PF (Deprecated)
// t_PF (Deprecated)

// t_C
template <typename t_CTest = int>
struct TCTest5;

// t_F
template <typename t_FTest = void (int)>
struct TCTest6;

// tp_C
template <typename ...tp_CTest> // = int
struct TCTest7;

// tp_F
template <typename ...tp_FTest> // = void (int)
struct TCTest8;

// pFunctionTemplateTypeParam:
// tf_PF (Deprecated)
// tfp_PF (Deprecated)

// tf_C
template <typename tf_CTest = int>
void fg_Test2();

// tf_F
template <typename tf_FTest = void (int)>
void fg_Test3();

// tfp_C
template <typename ...tfp_CTest> // = int
void fg_Test();

// tfp_F
template <typename ...tfp_FTest> // = void (int)
void fg_Test();


// pTemplateTemplateParam:
// t_T

template <template <typename> class t_TTest>
struct TCTest9;

// tp_T
template <template <typename> class ...tp_TTest>
struct TCTest10;

// pFunctionTemplateTemplateParam:
// tf_T
template <template <typename> class tf_TTest>
void fg_Test(TCTest10<tf_TTest>)
{
}

// tfp_T
template <template <typename> class ...tfp_TTest>
void fg_Test()
{
}

// pFunctionTemplateNonTypeParam:
// tf_
template <int tf_Test>
void fg_Test();

// tfp_
template <int ...tfp_Test>
void fg_Test();

