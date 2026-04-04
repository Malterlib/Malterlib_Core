// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <filesystem>

namespace
{
	class CStdLib_Tests : public NMib::NTest::CTest
	{
	public:

		void f_DoTests()
		{
			DMibTestSuite("FileSystem")
			{
				[[maybe_unused]] auto CurrentPath = std::filesystem::current_path();
				[[maybe_unused]] auto AbsolutePath = std::filesystem::absolute(".");
			};
		}
	};
}
DMibTestRegister(CStdLib_Tests, Malterlib::Core);
