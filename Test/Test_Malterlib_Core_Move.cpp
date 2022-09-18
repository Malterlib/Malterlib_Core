// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <string>
#include <tuple>
#include <map>

#include <Mib/Storage/Reference>
#include <Mib/String/Mixed>
#include <Mib/Numeric/fp128>
#include <Mib/Storage/Variant>

struct CTestStruct
{
	CTestStruct()
	{
		//DMibTrace("CTestStruct construct\r\n", 0);
	}
	CTestStruct(CTestStruct const &_Other)
	{
		//DMibTrace("CTestStruct copy\r\n", 0);
	}
	CTestStruct(CTestStruct &&_Other)
	{
		//DMibTrace("CTestStruct move\r\n", 0);
	}
};

struct CTestStruct2
{
	CTestStruct m_Member0;
	CTestStruct m_Member1;

	const ch8 *f_TestDebugger() const
	{
		return "Hahahaha";
	}
};


struct CTestLinked
{
	NMib::NStr::CStr m_Data;
	DMibListLinkD_Link(CTestLinked, m_Link);
	DMibListLinkS_Link(CTestLinked, m_SingleLink);

	CTestLinked()
	{
	}

	CTestLinked(CTestLinked &&_Other)
		: m_Data(NMib::fg_Move(_Other.m_Data))
		, m_Link(NMib::fg_Move(_Other.m_Link))
	{
	}
};

struct CTestAVLTree
{
	NMib::NStr::CStr m_Data;
	NMib::NIntrusive::TCAVLLink<> m_Link;

	class CCompare
	{
	public:
		inline_small NMib::NStr::CStr const & operator () (CTestAVLTree const &_Object) const
		{
			return _Object.m_Data;
		}
	};

	CTestAVLTree()
	{
	}
};



enum EParam
{
	EParam_void
	, EParam_int
	, EParam_CStr
	, EParam_CMStr
};

namespace
{
	class CMove_Tests : public NMib::NTest::CTest
	{
	public:

		void f_DoTests()
		{
#if 1
			CTestStruct2 Struct;
			Struct.f_TestDebugger();

			CTestStruct2 Struct2(NMib::fg_Move(Struct));

#ifdef DCompiler_MSVC
			typedef std::wstring CStdUTF16String;
#else
			typedef std::u16string CStdUTF16String;
#endif

			std::pair<std::string, CStdUTF16String> Test("Testing1", str_utf16("Testing2"));

			std::tuple<std::string, CStdUTF16String> Test33("Testing1", str_utf16("Testing2"));

			NMib::NStr::CMStrDeprecated Source(NMib::NStr::CWStr(str_utf16("Testing 日本語 !")));

			std::string strtest = "testing!!!";
			CStdUTF16String strtestw = str_utf16("testing!!!");
			NMib::NStr::CStr TestStr2;
			NMib::NStr::CStr TestStr(Source);

			TestStr2 = NMib::fg_Move(TestStr);

			ch8 const *pStr = "Testing!";

			NMib::NStr::CWStr TestStr3 = Source;
			NMib::NStr::CUStr TestStr4 = Source;

			NMib::NStr::CFStr128 TestStr5 = Source;
			NMib::NStr::CFWStr128 TestStr6 = Source;
			NMib::NStr::CFUStr128 TestStr7 = Source;

			NMib::NTime::CTime Time = NMib::NTime::CTime::fs_NowUTC();

			NMib::NTime::CTimeSpan TimeSpan = NMib::NTime::CTimeSpanConvert	::fs_CreateSpan(1,1,1,1,1,0.5);

			fp32 Test0 = 3.55f;
			fp64 Test1 = 3.55;
			fp128 Test2 = fp64(6.55);

			NMib::NStr::CMStrDeprecated Test334 = "Hahaha";
			NMib::NContainer::TCLinkedList<NMib::NStr::CMStrDeprecated> List;
			List.f_Insert(Test334);
			List.f_Insert(Test334);
			List.f_Insert(Test334);

			for (auto Iter = List.f_GetIterator(); Iter; ++Iter)
			{
				int y = 0;
			}


			NMib::NContainer::TCVector<CTestLinked> Vector;
			DMibListLinkD_List(CTestLinked, m_Link) LinkedList;
			DMibListLinkS_List(CTestLinked, m_SingleLink) LinkedListSingle;

			{
				auto &Insert0 = Vector.f_Insert();
				Insert0.m_Data = "Insert 0";
				LinkedList.f_Insert(Insert0);
				LinkedListSingle.f_Insert(Insert0);

				auto &Insert1 = Vector.f_Insert();
				Insert1.m_Data = "Insert 1";
				LinkedList.f_Insert(Insert1);
				LinkedListSingle.f_Insert(Insert1);

				auto &Insert2 = Vector.f_Insert();
				Insert2.m_Data = "Insert 2";
				LinkedList.f_Insert(Insert2);
				LinkedListSingle.f_Insert(Insert2);
			}
			for (auto Iter = LinkedList.f_GetIterator(); Iter; ++Iter)
			{
				int y = 0;
			}

			for (auto Iter = LinkedListSingle.f_GetIterator(); Iter; ++Iter)
			{
				int y = 0;
			}

			auto VectorIter = Vector.f_GetIterator();
			for (; VectorIter; ++VectorIter)
			{
				int y = 0;
			}


			NMib::NContainer::TCLinkedList<CTestAVLTree> AVLList;
			NMib::NIntrusive::TCAVLTree<&CTestAVLTree::m_Link, CTestAVLTree::CCompare> AVLTree;

			{
				auto &Insert0 = AVLList.f_Insert();
				Insert0.m_Data = "Insert 0";
				AVLTree.f_Insert(Insert0);

				auto &Insert1 = AVLList.f_Insert();
				Insert1.m_Data = "Insert 1";
				AVLTree.f_Insert(Insert1);

				auto &Insert2 = AVLList.f_Insert();
				Insert2.m_Data = "Insert 2";
				AVLTree.f_Insert(Insert2);
			}

			for (auto Iter = AVLTree.f_GetIterator(); Iter; ++Iter)
			{
				int y = 0;
			}

			NMib::NContainer::TCMap<NMib::NStr::CStr, NMib::NStr::CStr> Map;

			Map["Test 1"] = "3";
			Map["Test 2"] = "2";
			Map["Test 3"] = "1";

			for (auto Iter = Map.f_GetIterator(); Iter; ++Iter)
			{
				int y = 0;
			}

			NMib::NContainer::TCSet<NMib::NStr::CStr> Set;

			Set["Test 1"];
			Set["Test 2"];
			Set["Test 3"];

			for (auto Iter = Set.f_GetIterator(); Iter; ++Iter)
			{
				int y = 0;
			}

			NMib::NStorage::TCAggregateSimple<NMib::NStr::CStr> TestAggregateSimple = {DAggregateInit};
			TestAggregateSimple.f_Construct();
			*TestAggregateSimple = "Test aggregate";
			TestAggregateSimple.f_Destruct();

			{
				NMib::NStorage::TCAggregate<NMib::NStr::CStr> TestAggregate = {DAggregateInit};
				*TestAggregate = "Test aggregate";
				int x = 0;
				TestAggregate.f_Clear();
				*TestAggregate = "Test aggregate";
				TestAggregate.f_Destruct();
				int y = 0;
			}

			NMib::NStorage::TCStreamableVariant
				<
					EParam
					, NMib::NStorage::TCMember<void, EParam_void>
					, NMib::NStorage::TCMember<int, EParam_int>
					, NMib::NStorage::TCMember<NMib::NStr::CStr, EParam_CStr>
					, NMib::NStorage::TCMember<NMib::NStr::CMStrDeprecated, EParam_CMStr>
				>
				Variant0
			;

			Variant0 = 5;
			Variant0 = "Test 55";
			Variant0 = NMib::NStr::CMStrDeprecated("Testing 66");

#endif
			NMib::NContainer::CRegistry Registry;
			auto *pChild = Registry.f_CreateChild("RootChild");
			pChild->f_SetValue("test", "Mega");
			pChild->f_SetValue("test1", "Test ");
			pChild->f_SetValue("test2", "66");
			pChild->f_SetValue("test3", "77");


			NMib::NThread::TCThreadLocal<NMib::NStr::CStr> ThreadLocal;

			*ThreadLocal = "Threaded!!!";

			auto Ref = NMib::fg_Reference(TestStr2);
			auto UndefinedRef = NMib::fg_Reference(TestStr2);
		}

	};

	DMibTestRegister(CMove_Tests, Malterlib::Core);
}

