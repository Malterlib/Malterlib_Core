// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib
{
	[[noreturn]] void fg_NoReturn()
	{
		throw 1;
	}

	namespace NMisc
	{
		struct CSubSystem_Misc_Random : public CSubSystem
		{
			void f_ForkedChild() override
			{
				// Reset random
				m_Random.f_ReinitForThread();
			}

			NThread::TCThreadLocal<CAutoRandom, NMemory::CAllocator_NonTrackedHeap> m_Random;
		};

		constinit TCSubSystem<CSubSystem_Misc_Random, ESubSystemDestruction_BeforeMemoryManager> g_SubSystem_Misc_Random = {DAggregateInit};

		CRandomShiftRNG &fg_RandomThreadLocal()
		{
			return *(*g_SubSystem_Misc_Random).m_Random;
		}

		CAutoRandom::CAutoRandom()
			: CRandomShiftRNG(uint32(NTime::NPlatform::fg_Timer_Cycles() & uint64(0xffffffff)), uint32((NTime::NPlatform::fg_Timer_Cycles() >> 32) & uint64(0xffffffff)), uint32(NTime::NPlatform::fg_Timer_Cycles() & uint64(0xffffffff)))
		{
		}

		NStr::CStr fg_FormatTime(const NTime::CTime &_Time)
		{
			NTime::CTimeConvert::CDateTime DateTime;
			NTime::CTimeConvert(_Time).f_ExtractDateTime(DateTime);

			return NStr::CStr::CFormat("{}-{sj2,sf0}-{sj2,sf0} {sj2,sf0}:{sj2,sf0}:{sj2,sf0}.{sj3,sf0,fe3}") << DateTime.m_Year << DateTime.m_Month << DateTime.m_DayOfMonth << DateTime.m_Hour << DateTime.m_Minute << DateTime.m_Second << DateTime.m_Fraction * 1000.0;
		}

		template <typename tf_CStr>
		tf_CStr fg_FormatTimeFileNameTemplated(const NTime::CTime &_Time)
		{
			NTime::CTimeConvert::CDateTime DateTime;
			NTime::CTimeConvert(_Time).f_ExtractDateTime(DateTime);

			return typename tf_CStr::CFormat("{}-{sj2,sf0}-{sj2,sf0}_{sj2,sf0}.{sj2,sf0}.{sj2,sf0}.{sj3,sf0,fe3}") << DateTime.m_Year << DateTime.m_Month << DateTime.m_DayOfMonth << DateTime.m_Hour << DateTime.m_Minute << DateTime.m_Second << DateTime.m_Fraction * 1000.0;
		}

		NStr::CStr fg_FormatTimeFileName(const NTime::CTime &_Time)
		{
			NTime::CTimeConvert::CDateTime DateTime;
			NTime::CTimeConvert(_Time).f_ExtractDateTime(DateTime);

			return NStr::CStr::CFormat("{}-{sj2,sf0}-{sj2,sf0}_{sj2,sf0}.{sj2,sf0}.{sj2,sf0}.{sj3,sf0,fe3}") << DateTime.m_Year << DateTime.m_Month << DateTime.m_DayOfMonth << DateTime.m_Hour << DateTime.m_Minute << DateTime.m_Second << DateTime.m_Fraction * 1000.0;
		}

		bool fg_CheckFileAccessRights(NStr::CStr _Path)
		{
			try
			{

				{
					NFile::CFile File;
					File.f_Open(_Path, NFile::EFileOpen_Write | NFile::EFileOpen_NoLocalCache | NFile::EFileOpen_ShareAll);

					uint32 Test = 1;
					File.f_Write(&Test, sizeof(Test));
				}

				return true;

			}
			catch (NFile::CExceptionFile)
			{
				return false;
			}
		}

		template <typename tf_CStr>
		bool fg_CheckAccessRightsTemplated(tf_CStr const& _Path, bool _bRandom)
		{
			try
			{
				tf_CStr GUID;

				if (_bRandom)
					GUID = typename tf_CStr::CFormat("TestAccessRights.{}.{}") << NMisc::fg_GetRandom() << fg_FormatTimeFileNameTemplated<tf_CStr>(NTime::CTime::fs_NowUTC());
				else
					GUID = typename tf_CStr::CFormat("TestAccessRights.{}") << fg_FormatTimeFileNameTemplated<tf_CStr>(NTime::CTime::fs_NowUTC());


				NFile::CFile::fs_CreateDirectory(_Path);
				NFile::CFile::fs_CreateDirectory(_Path + "/" + GUID);
				NFile::CFile::fs_DeleteDirectory(_Path + "/" + GUID);

				{
					NFile::CFile File;
					File.f_Open(_Path +"/"+ GUID + ".file", NFile::EFileOpen_Write | NFile::EFileOpen_NoLocalCache | NFile::EFileOpen_ShareAll);

					uint32 Test = 1;
					File.f_Write(&Test, sizeof(Test));
				}

				NFile::CFile::fs_DeleteFile(_Path +"/"+ GUID + ".file");

				return true;

			}
			catch (NFile::CExceptionFile)
			{
				return false;
			}
		}

		bool fg_CheckAccessRights(NStr::CStrNonTracked const& _Path, bool _bRandom)
		{
			return fg_CheckAccessRightsTemplated(_Path, _bRandom);
		}

		bool fg_CheckAccessRights(NStr::CStr const& _Path, bool _bRandom)
		{
			return fg_CheckAccessRightsTemplated(_Path, _bRandom);
		}

		CClassContainerList *fg_GetClassContainerListArgList(CClassContainerList &_List, CMibArgList &_Args)
		{
			void *CurrentPtr = DMibPArgListNextArg(_Args, void *);

			while(CurrentPtr)
			{
//#ifdef DDebug
				// TODO: Add dynamic cast fix
//				DSafeCheck(dynamic_cast<CClassContainer *>((CClassContainer *) CurrentPtr), "Not a class container");
//#endif
				_List.m_List.f_Insert((CClassContainer *) CurrentPtr);

				CurrentPtr = DMibPArgListNextArg(_Args, void *);
			}

			return &_List;
		}

		CClassContainerList *fg_GetClassContainerList(CClassContainerList *_pList, ...)
		{
			CMibArgList Args;
			DMibPArgListStart(Args, _pList);

			return fg_GetClassContainerListArgList(*_pList, Args);
		}
	}
}
