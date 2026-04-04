// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include <Mib/Encoding/EJson>
#include <Mib/Encoding/JsonGenerate>
#include <Mib/Encoding/JsonShortcuts>

#include <sys/wait.h>
#include <unistd.h>

#if defined(DPlatformFamily_Linux)
	#include <client/linux/handler/exception_handler.h>
#else
	#include <client/mac/handler/exception_handler.h>
#endif

namespace NMib
{
	struct CBreakpad
	{
		NContainer::TCVector<NStr::CStr> m_Env;
		NContainer::TCVector<ch8 *> m_EnvList;

		CBreakpad()
		{
			auto BuildMetadata = NSys::fg_GetBuildMetadata();

			NEncoding::CEJsonSorted Metadata
				{
					"Product"_= BuildMetadata.m_pProduct
					, "Application"_= BuildMetadata.m_pApplication
					, "Configuration"_= BuildMetadata.m_pConfiguration
					, "GitBranch"_= BuildMetadata.m_pGitBranch
					, "GitCommit"_= BuildMetadata.m_pGitCommit
					, "Platform"_= BuildMetadata.m_pPlatform
					, "Version"_= BuildMetadata.m_pVersion
#if defined(DMibContract_AnyEnabled) || DMibEnableSafeCheck > 0
					, "ExceptionInfo"_= "{ExceptionInfo}"
#endif
				}
			;

			auto &OutTags = Metadata["Tags"].f_Array();

			for (umint iTag = 0; iTag < BuildMetadata.m_nTags; ++iTag)
				OutTags.f_Insert(BuildMetadata.m_pTags[iTag]);

			m_MetadataTemplate = Metadata.f_ToString();

			auto FinalEnv = fg_GetSys()->f_Environment();
			for (auto iEnv = FinalEnv.f_GetIterator(); iEnv; ++iEnv)
				m_EnvList.f_Insert(m_Env.f_Insert(fg_Format("{}={}", iEnv.f_GetKey(), *iEnv)).f_GetStrUniqueWritable());
			m_EnvList.f_Insert((ch8 *)nullptr);

			NStr::CStrNonTracked DumpLocation;
			{
				try
				{
					bool bUseMalterlibCrashDumpDir = false;

					NStr::CStrNonTracked MalterlibCrashDumpDir = NSys::fg_Process_GetEnvironmentVariable_NonProtected(NStr::CStrNonTracked("MalterlibCrashDumpDir"));
					if (!MalterlibCrashDumpDir.f_IsEmpty())
					{
						if (NMisc::fg_CheckAccessRights(MalterlibCrashDumpDir, false))
						{
							DumpLocation = MalterlibCrashDumpDir;
							bUseMalterlibCrashDumpDir = true;
						}
					}

					if (!bUseMalterlibCrashDumpDir)
					{
						DumpLocation = NFile::CFile::fs_AppendPath(NFile::CFile::fs_GetProgramDirectoryNonTracked(), "CrashDumps");
						if (!NMisc::fg_CheckAccessRights(DumpLocation, false))
						{
							DumpLocation = NFile::CFile::fs_AppendPath(NFile::CFile::fs_GetUserLocalProgramDirectoryNonTracked(), "CrashDumps");
							if (!NMisc::fg_CheckAccessRights(DumpLocation, false))
								DumpLocation = "/tmp";
						}
					}

					NFile::CFile::fs_CreateDirectory(DumpLocation);
				}
				catch (NFile::CExceptionFile const&)
				{
					DumpLocation = "/tmp";
				}
			}

#if defined(DPlatformFamily_Linux)
			google_breakpad::MinidumpDescriptor Descriptor(DumpLocation.f_GetStr());
			m_pExceptionHandler = fg_Construct(Descriptor
											   , nullptr
											   , fs_DumpCallback
											   , this
											   , true
											   , -1);
#else
			m_pExceptionHandler = fg_Construct(DumpLocation.f_GetStr()
											   , nullptr
											   , fs_DumpCallback
											   , this
											   , true
											   , nullptr);
#endif
		}

		~CBreakpad()
		{
			m_pExceptionHandler.f_Clear();
		}

#if defined(DPlatformFamily_Linux)
		static bool fs_DumpCallback(google_breakpad::MinidumpDescriptor const &_Descriptor, void *_pContext, bool _bSucceeded)
#else
		static bool fs_DumpCallback(char const *_pDumpDir, char const *_pMiniDumpID, void *_pContext, bool _bSucceeded)
#endif
		{
			auto pThis = (CBreakpad *)_pContext;

#if defined(DPlatformFamily_Linux)
			NStr::CStrNonTracked DumpPath(_Descriptor.path());
#else
			NStr::CStrNonTracked DumpPath = NStr::CStrNonTracked::CFormat("{}/{}.dmp") << _pDumpDir << _pMiniDumpID;
#endif
			NSys::fg_DebugOutput((NStr::CStrNonTracked::CFormat("Application crashed. Saving crash dump to: {}\n") << DumpPath).f_GetStr().f_GetStr());

			NStr::CStrNonTracked MetadataPath = NFile::CFile::fs_GetPath(DumpPath) / (NFile::CFile::fs_GetFileNoExt(DumpPath) + ".json");

			{
				auto Metadata = pThis->m_MetadataTemplate;
#if defined(DMibContract_AnyEnabled) || DMibEnableSafeCheck > 0
				NStr::CStrNonTracked ExceptionInfoJson;
				{
					NStr::CStrNonTracked::CAppender Appender(ExceptionInfoJson);

					auto ExceptionInfo = NSys::fg_System_GetContractViolationMessage();
					NEncoding::NJson::fg_GenerateJsonString<'\"', NEncoding::NJson::CParseContext, false>(Appender, ExceptionInfo);
				}
				Metadata = Metadata.f_Replace("{ExceptionInfo}", ExceptionInfoJson);
#endif
				NFile::CFile::fs_WriteStringToFile(MetadataPath, Metadata, false);
			}

			if (!_bSucceeded || fg_GetSys()->f_GetCrashHandlerPath().f_IsEmpty())
			{
				_exit(1);
				return _bSucceeded; // Failed to generate minidump or we specifically don't want to spawn a reporter (eg when running the server)
			}

			pid_t Child = fork();

			if (Child == 0)
			{
				// Child process.

				NStr::CStrNonTracked const& ProgramName = fg_GetSys()->f_GetProgramNameNonTracked();

				const char* pArgs[] =
					{
						fg_GetSys()->f_GetCrashHandlerPath().f_GetStr()
						, ProgramName.f_IsEmpty() ? "application" : ProgramName.f_GetStr()
						, "ErrorReporter"
						, DumpPath.f_GetStr()
						, fg_GetSys()->f_GetCrashHandlerExePath()
						, fg_GetSys()->f_GetCrashHandlerServer()
						, NULL
					}
				;

				execve(pArgs[0], const_cast<char**>(pArgs), pThis->m_EnvList.f_GetArray());
				NSys::fg_TerminateProcess(1);
			}
			else if (Child > 0)
			{
				waitpid(Child, nullptr, WNOHANG);
			}

			_exit(1);

			return _bSucceeded;
		}

		NStorage::TCUniquePointer<google_breakpad::ExceptionHandler, NMemory::CAllocator_NonTrackedHeap> m_pExceptionHandler;
		NStr::CStrNonTracked m_MetadataTemplate;
	};

	namespace NSys
	{
		void fg_InitialiseCrashReporter();

		constinit NMib::NStorage::TCAggregateSimple<CBreakpad> g_Breakpad = {DAggregateInit};

		void fg_InitBreakpad()
		{
			g_Breakpad.f_Construct();
			fg_InitialiseCrashReporter();
		}
		void fg_DestroyBreakpad()
		{
			g_Breakpad.f_Destruct();
		}
	}
} // Namespace NMib
