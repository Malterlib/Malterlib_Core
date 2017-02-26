// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

//namespace NOperators
//{

//}

//using namespace NMib;


namespace
{
	class CTestVal
	{
		int32 m_Value;
	public:
		static int32 &fsp_ValueType();
		CTestVal(int32 _Value)
			: m_Value(_Value)
		{
		}

		int f_GetVal() const
		{
			return m_Value;
		}

		operator bool () const
		{
			return m_Value != 0;
		}

		bint operator < (CTestVal const &_Other) const
		{
			return m_Value < _Other.m_Value;
		}

		bint operator == (CTestVal const &_Other) const
		{
			return m_Value == _Other.m_Value;
		}

		bint operator < (int _Other) const
		{
			return m_Value < _Other;
		}

		bint operator == (int _Other) const
		{
			return m_Value == _Other;
		}

		template <typename t_CFormatter>
		auto f_GetStringFormatType(t_CFormatter &_Formatter) -> decltype(NMib::NStr::fg_GetStringFormatType(_Formatter, fsp_ValueType()));

		template <typename t_CFormatter>
		auto f_CreateStringFormatter(t_CFormatter &_Formatter) const -> decltype(NMib::NStr::fg_CreateStringFormatter(_Formatter, fsp_ValueType()))
		{
			return NMib::NStr::fg_CreateStringFormatter(_Formatter, m_Value);
		}
	};

	bint operator < (int _Left, const CTestVal &_Right)
	{
		return _Left < _Right.f_GetVal();
	}

	bint operator == (int _Left, const CTestVal &_Right)
	{
		return _Left == _Right.f_GetVal();
	}

	enum ETestOperator
	{
		ETestOperator_0 = DMibBit(0),
		ETestOperator_1 = DMibBit(1),
		ETestOperator_2 = DMibBit(2),
		ETestOperator_All = ETestOperator_0 | ETestOperator_1 | ETestOperator_2,
		ETestOperator_LeftTest1 = 3 << 2,
		ETestOperator_LeftTest2 = 3 << 4,
		ETestOperator_Negative = -1
	};



	DMibStaticCheck(ETestOperator_All == 7);

	class COperators_Tests : public NMib::NTest::CTest
	{
	public:

		void f_DoTests()
		{

			DMibTestCategory("Enums")
			{
				DMibTestSuite("Bitwise Or")
				{
					ETestOperator EnumValue = ETestOperator_0 | ETestOperator_1;
					DMibTest(DMibExpr(EnumValue == 3));
					EnumValue |= ETestOperator_2;
					DMibTest(DMibExpr(EnumValue == 7));
				};

				DMibTestSuite("Bitwise And")
				{
					ETestOperator EnumValue = ETestOperator_0 | ETestOperator_1;
					EnumValue = EnumValue & ETestOperator_1;
					DMibTest(DMibExpr(EnumValue == ETestOperator_1));
					EnumValue &= ETestOperator_0;
					DMibTest(DMibExpr(EnumValue == 0));
				};

				DMibTestSuite("Bitwise Exclusive Or")
				{
					ETestOperator EnumValue = ETestOperator_0 | ETestOperator_1;
					EnumValue = EnumValue ^ ETestOperator_1;
					DMibTest(DMibExpr(EnumValue == ETestOperator_0));
					EnumValue ^= ETestOperator_1;
					DMibTest(DMibExpr(EnumValue == 3));
				};

				DMibTestSuite("Bitwise Complement")
				{
					ETestOperator EnumValue = ETestOperator_0 | ETestOperator_1;
					EnumValue = ~EnumValue;
					DMibTest(DMibExpr(EnumValue == ETestOperator(~uint32(3))));
				};

				DMibTestSuite("Bitwise Shift Left")
				{
					ETestOperator EnumValue = ETestOperator_0 | ETestOperator_1;
					EnumValue = EnumValue << 2;
					DMibTest(DMibExpr(EnumValue == 3 << 2));
					EnumValue <<= ETestOperator_1;
					DMibTest(DMibExpr(EnumValue == 3 << 4));
				};

				DMibTestSuite("Bitwise Shift Right")
				{
					ETestOperator EnumValue = ETestOperator_0 | ETestOperator_1 | ETestOperator_2;
					EnumValue = EnumValue >> 1;
					DMibTest(DMibExpr(EnumValue) == DMibExpr(7 >> 1));
					EnumValue >>= ETestOperator_1;
					DMibTest(DMibExpr(EnumValue) == DMibExpr(7 >> 3));
				};

				// Negative enums seems to have another type when you disable language extensions, which is strange. Will have to check the standard.
	#if 0
				DMibTestSuite("Negative")
				{
					ETestOperator EnumValue = ETestOperator_0 | ETestOperator_Negative;
					DMibTest(DMibExpr(EnumValue == -1));
					EnumValue = ETestOperator_Negative | ETestOperator_0;
					DMibTest(DMibExpr(EnumValue == -1));
					EnumValue = ETestOperator_0;
					EnumValue |= ETestOperator_Negative;
					DMibTest(DMibExpr(EnumValue == -1));
				};
	#endif

			};

			DMibTestCategory("Comparison")
			{
				DMibTestSuite("Built in types")
				{
					DMibTest(DMibExpr(CTestVal(3)) > DMibExpr(2));
					DMibTest(DMibExpr(3) > DMibExpr(CTestVal(2)));

					//DMibDTrace("{}", CTestVal(2));
				};

				DMibTestSuite("Equal")
				{
					DMibTest(DMibExpr(CTestVal(2)) == DMibExpr(CTestVal(2)));
					DMibTest(DMibExpr(!(CTestVal(3) == CTestVal(1))));
					DMibTest(DMibExpr(!(CTestVal(1) == CTestVal(3))));
				};

				DMibTestSuite("Not Equal")
				{
					DMibTest(DMibExpr(CTestVal(3)) != DMibExpr(CTestVal(1)));
					DMibTest(DMibExpr(CTestVal(1)) != DMibExpr(CTestVal(3)));
					DMibTest(DMibExpr(!(CTestVal(2) != CTestVal(2))));
				};


				DMibTestSuite("Greater Than")
				{
					DMibTest(DMibExpr(CTestVal(3)) > DMibExpr(CTestVal(1)));
					DMibTest(DMibExpr(!(CTestVal(2) > CTestVal(2))));
					DMibTest(DMibExpr(!(CTestVal(1) > CTestVal(3))));
				};

				DMibTestSuite("Less than")
				{
					DMibTest(DMibExpr(CTestVal(1)) < DMibExpr(CTestVal(3)));
					DMibTest(DMibExpr(!(CTestVal(3) < CTestVal(1))));
					DMibTest(DMibExpr(!(CTestVal(2) < CTestVal(2))));
				};

				DMibTestSuite("Less than equal")
				{
					DMibTest(DMibExpr(CTestVal(1)) <= DMibExpr(CTestVal(3)));
					DMibTest(DMibExpr(CTestVal(2)) <= DMibExpr(CTestVal(3)));
					DMibTest(DMibExpr(CTestVal(3)) <= DMibExpr(CTestVal(3)));
					DMibTest(DMibExpr(!(CTestVal(3) <= CTestVal(2))));
				};

				DMibTestSuite("Greater than equal")
				{
					DMibTest(DMibExpr(CTestVal(3)) >= DMibExpr(CTestVal(3)));
					DMibTest(DMibExpr(CTestVal(4)) >= DMibExpr(CTestVal(3)));
					DMibTest(DMibExpr(CTestVal(5)) >= DMibExpr(CTestVal(3)));
					DMibTest(DMibExpr(!(CTestVal(2) >= CTestVal(3))));
				};
			};

			DMibTestSuite("Implicit Conversion")
			{
				bint bTest = fp64(10.0) < -1000000000.0f;
				bTest = fp64(10.0) < -1000000000.0;
				bTest = 10.0f < fp64(-1000000000.0);
				bTest = 10.0 < fp64(-1000000000.0);
				bTest = fp32(10.0) < fp64(-1000000000.0);
				bTest = fp64(10.0) < fp32(-1000000000.0);

#if 0
				DMibTrace("{}\r\n", sizeof(fp64(10.0) * fp32(1.0f/3.0f)));
				DMibTrace("{}\r\n", sizeof(fp32(10.0f) * fp64(1.0/3.0)));
				DMibTrace("{}\r\n", fp64(10.0) * fp32(1.0f/3.0f));
				DMibTrace("{}\r\n", fp32(10.0f) * fp64(1.0/3.0));
#endif

				fp64 fFloat = 1.1111111111111111111111111111111111111111111111111111111111;
				fFloat = NMib::fg_Clamp(fp64(fp32(fFloat)), -1000000000.0, 1000000000.0);

			};
									

		}
	};
	
	DMibTestRegister(COperators_Tests, Malterlib::Core);
}


