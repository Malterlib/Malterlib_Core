// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <sys/wait.h>
#include <unistd.h>

#if defined(DPlatformFamily_Linux)
	#include <client/linux/handler/exception_handler.h>
#else
	#include <client/mac/handler/exception_handler.h>
#endif

namespace NMib
{
	namespace NSys
	{
		void fg_InitialiseCrashReporter();
		
		ch8 **g_LaunchEnvironment = nullptr;
		
#if defined(DPlatformFamily_Linux)
		static bool fsg_DumpCallback(google_breakpad::MinidumpDescriptor const &_Descriptor, void* _pContext, bool _bSucceeded)
#else
		static bool fsg_DumpCallback(const char* _pDumpDir, const char* _pMiniDumpID, void* _pContext, bool _bSucceeded)
#endif
		{
#if defined(DPlatformFamily_Linux)
			ch8 const *pDumpPath = _Descriptor.path();
#else
			NMib::NStr::CFStr1024 DumpPath = NMib::NStr::CFStr1024::CFormat("{}/{}.dmp") << _pDumpDir << _pMiniDumpID;
			ch8 const *pDumpPath = DumpPath;
#endif
			NMib::NSys::fg_DebugOutput((NMib::NStr::CFStr1024::CFormat("Application crashed. Saving crash dump to: {}\n") << pDumpPath).f_GetStr().f_GetStr());

			if (!_bSucceeded || NMib::fg_GetSys()->f_GetCrashHandlerPath().f_IsEmpty())
				return _bSucceeded; // Failed to generate minidump or we specifically don't want to spawn a reporter (eg when running the server)

			pid_t Child = fork();
			
			if (Child == 0)
			{
				// Child process.
				
				NMib::NStr::CStrNonTracked const& ProgramName = NMib::fg_GetSys()->f_GetProgramNameNonTracked();
				
				const char* pArgs[] =
					{
						NMib::fg_GetSys()->f_GetCrashHandlerPath().f_GetStr()
						, ProgramName.f_IsEmpty() ? "application" : ProgramName.f_GetStr()
						, "ErrorReporter"
						, pDumpPath
						, NMib::fg_GetSys()->f_GetCrashHandlerExePath()
						, NMib::fg_GetSys()->f_GetCrashHandlerServer()
						, NULL
					}
				;
				
				execve(pArgs[0], const_cast<char**>(pArgs), g_LaunchEnvironment);
				NMib::NSys::fg_TerminateProcess(1);
			}
			else if (Child > 0)
			{
				waitpid(Child, nullptr, WNOHANG);
			}
					
			return _bSucceeded;
		}
		
		struct CBreakpad
		{		
			NContainer::TCVector<NMib::NStr::CStr> m_Env;
			NContainer::TCVector<ch8 *> m_EnvList;
			CBreakpad()
			{
				auto FinalEnv = fg_GetSys()->f_Environment();
				for (auto iEnv = FinalEnv.f_GetIterator(); iEnv; ++iEnv)
					m_EnvList.f_Insert(m_Env.f_Insert(fg_Format("{}={}", iEnv.f_GetKey(), *iEnv)).f_GetStrUniqueWritable());
				m_EnvList.f_Insert((ch8 *)nullptr);
				g_LaunchEnvironment = m_EnvList.f_GetArray();  
				
				NMib::NStr::CStrNonTracked DumpLocation;
				{
					try
					{
						bool bUseMalterlibCrashDumpDir = false;

						NMib::NStr::CStrNonTracked MalterlibCrashDumpDir = NSys::fg_Process_GetEnvironmentVariable_NonProtected(NMib::NStr::CStrNonTracked("MalterlibCrashDumpDir"));
						if (!MalterlibCrashDumpDir.f_IsEmpty())
						{
							if (NMib::NMisc::fg_CheckAccessRights(MalterlibCrashDumpDir, false))
							{
								DumpLocation = MalterlibCrashDumpDir;
								bUseMalterlibCrashDumpDir = true;
							}
						}

						if (!bUseMalterlibCrashDumpDir)
						{
							DumpLocation = NMib::NFile::CFile::fs_AppendPath(NMib::NFile::CFile::fs_GetProgramDirectoryNonTracked(), "CrashDumps");
							if (!NMib::NMisc::fg_CheckAccessRights(DumpLocation, false))
							{
								DumpLocation = NMib::NFile::CFile::fs_AppendPath(NMib::NFile::CFile::fs_GetUserLocalProgramDirectoryNonTracked(), "CrashDumps");
								if (!NMib::NMisc::fg_CheckAccessRights(DumpLocation, false))
									DumpLocation = "/tmp";
							}
						}
						
						NMib::NFile::CFile::fs_CreateDirectory(DumpLocation);
					}
					catch (NMib::NFile::CExceptionFile const&)
					{
						DumpLocation = "/tmp";
					}
				}
										
#if defined(DPlatformFamily_Linux)
				google_breakpad::MinidumpDescriptor Descriptor(DumpLocation.f_GetStr());
				m_pExceptionHandler = fg_Construct(Descriptor
												   , nullptr
												   , fsg_DumpCallback
												   , nullptr
												   , true
												   , -1);
#else
				m_pExceptionHandler = fg_Construct(DumpLocation.f_GetStr()
												   , nullptr
												   , fsg_DumpCallback
												   , nullptr
												   , true
												   , nullptr);
#endif
			}
			
			~CBreakpad()
			{
				m_pExceptionHandler = nullptr;
				g_LaunchEnvironment = nullptr;
			}
					
			NMib::NStorage::TCUniquePointer<google_breakpad::ExceptionHandler, NMib::NMemory::CAllocator_NonTrackedHeap> m_pExceptionHandler;
		};
		
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
