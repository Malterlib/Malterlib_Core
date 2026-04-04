// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#ifdef DPlatformFamily_macOS
#include <mach/task_policy.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#endif

#ifdef DPlatformFamily_Linux
#include <sys/time.h>
#include <sys/resource.h>
#endif

extern char **environ;

namespace
{
	void fg_FormatErrno(char *_pOutput, int _Err)
	{
		char Buffer[257];

		char const *pError = nullptr;

#if defined(DPlatformFamily_Linux) || defined(DPlatformFamily_Emscripten)
		pError = strerror_r(_Err, Buffer, 256); // IMPORTANT: This function has different semantics on macOS and Linux
#elif defined(DPlatformFamily_macOS)
		if (!strerror_r(_Err, Buffer, 256)) // IMPORTANT: This function has different semantics on macOS and Linux
			pError = Buffer;
#else
	#error "Not implemented"
#endif

		if (!pError)
			snprintf(_pOutput, 65536, "Unknown error (%d)", _Err);
		else
			snprintf(_pOutput, 65536, "%s (%d)", pError, _Err);
	}

	template <typename ...tfp_CArgs>
	void fg_OutputError(int _ErrNo, char const *_pFormat, tfp_CArgs && ...p_Args)
	{
		char ErrorBuffer[65537];
		fg_FormatErrno(ErrorBuffer, _ErrNo);

		fprintf(stderr, _pFormat, p_Args..., ErrorBuffer);
	}

	bool fg_StartsWith(char const *_pToCheck, char const *_pStartsWith)
	{
		auto StartsWithLen = strlen(_pStartsWith);
		if (strlen(_pToCheck) < StartsWithLen)
			return false;

		return strncmp(_pToCheck, _pStartsWith, strlen(_pStartsWith)) == 0;
	}

	char const *fg_GetEnvironmentValue(char const *_pEnvVar, char const *_pPrefix)
	{
		if (fg_StartsWith(_pEnvVar, _pPrefix))
			return _pEnvVar + strlen(_pPrefix);
		else
			return nullptr;
	}

	enum EExecutionPriority
	{
		EExecutionPriority_Lowest			= 0
		, EExecutionPriority_Low			= 0x2AAA
		, EExecutionPriority_BelowNormal	= 0x5555
		, EExecutionPriority_Normal			= 0x8000
		, EExecutionPriority_AboveNormal	= 0xAAAA
		, EExecutionPriority_High			= 0xD555
		, EExecutionPriority_Highest		= 0xFFFF
		, EExecutionPriority_Default		= -1
	};

#ifdef DPlatformFamily_Linux
	static constexpr int gc_MaxNice = 19;
#else
	static constexpr int gc_MaxNice = 20;
#endif

	int fg_Process_GetNice(EExecutionPriority _Priority)
	{
		if (_Priority < EExecutionPriority_Normal)
			return ((0x8000 - int(_Priority)) * gc_MaxNice) / 0x8000;
		else
			return (((0x8000 - int(_Priority + 1)) * 20) / 0x8000);
	}

	void fg_Process_SetPriority(EExecutionPriority _Priority)
	{
		// Best effort for setting priority
		for (int NicePriority = fg_Process_GetNice(_Priority); NicePriority <= 20; ++NicePriority)
		{
			if (!setpriority(PRIO_PROCESS, getpid(), NicePriority))
				break;
		}

#ifdef DPlatformFamily_macOS
		{
			struct task_category_policy TaskCategoryPolity;
			if (_Priority > EExecutionPriority_Normal)
				TaskCategoryPolity.role = TASK_FOREGROUND_APPLICATION;
			else if (_Priority == EExecutionPriority_Normal)
				TaskCategoryPolity.role = TASK_UNSPECIFIED;
			else
				TaskCategoryPolity.role = TASK_BACKGROUND_APPLICATION;
			task_policy_set(mach_task_self(), TASK_CATEGORY_POLICY, (thread_policy_t)&TaskCategoryPolity, TASK_CATEGORY_POLICY_COUNT);
		}
#endif
	}

	template <typename t_CData>
	constexpr bool fg_CharIsNumber(const t_CData _Character)
	{
		if (_Character >= '0' && _Character <= '9')
			return true;

		return false;
	}

	template <typename t_CChar>
	constexpr void fg_ParseNumeric(const t_CChar * &_pParse)
	{
		auto *pParse = _pParse;
		while (*pParse)
		{
			if (!fg_CharIsNumber(*pParse))
				break;
			++pParse;
		}

		_pParse = (const t_CChar * )pParse;
	}

	long long int fg_StrToInt(char const *_pParse, size_t _nChars)
	{
		char Temp[65];
		memset(Temp, 0, 65);
		if (_nChars > 64)
			_nChars = 64;

		strncpy(Temp, _pParse, _nChars);
		return atoll(Temp);
	}
}

int main(int _nArgs, char *_Arguments[])
{
	if (_nArgs < 2)
	{
		fprintf(stderr, "Expected option\n");
		return 1;
	}

	if (strcmp(_Arguments[1], "--malterlib-launch") == 0)
	{
		char **pEnvironment = environ;

		if (strcmp(*pEnvironment, "MalterlibLaunch=true") != 0)
		{
			fprintf(stderr, "Expected environment to start with MalterlibLaunch=true\n");
			return 1;
		}

		char const *pExecutableToLaunch = nullptr;

		++pEnvironment;

		bool bFoundEnd = false;

		while (*pEnvironment)
		{
			if (strcmp(*pEnvironment, "MalterlibLaunchEnd=true") == 0)
			{
				bFoundEnd = true;
				++pEnvironment;
				break;
			}

			if (auto pExecutable = fg_GetEnvironmentValue(*pEnvironment, "MalterlibLaunch_Executable="))
				pExecutableToLaunch = pExecutable;
			else if (auto pDirectory = fg_GetEnvironmentValue(*pEnvironment, "MalterlibLaunch_WorkingDirectory="))
			{
				if (chdir(pDirectory))
				{
					fg_OutputError(errno, "The OS returned an error from chdir('%s') when setting current directory in launch helper: %s\n", pDirectory);
					return 1;
				}
			}
			else if (auto pPriority = fg_GetEnvironmentValue(*pEnvironment, "MalterlibLaunch_Priority="))
				fg_Process_SetPriority((EExecutionPriority)atoll(pPriority));
			else if (auto pGid = fg_GetEnvironmentValue(*pEnvironment, "MalterlibLaunch_SetGid="))
			{
				if (setgid(atoll(pGid)))
				{
					fg_OutputError(errno, "The OS returned an error from setgid(%s) when setting process group in launch helper: %s\n", pGid);
					return 65;
				}
			}
			else if (auto pUid = fg_GetEnvironmentValue(*pEnvironment, "MalterlibLaunch_SetUid="))
			{
				if (setuid(atoll(pUid)))
				{
					fg_OutputError(errno, "The OS returned an error from setuid(%s) when setting process user in launch helper: %s\n", pUid);
					return 64;
				}
			}
			else if (auto pLimits = fg_GetEnvironmentValue(*pEnvironment, "MalterlibLaunch_Limits="))
			{
				auto pParse = pLimits;

				auto fReportParseError = [&](char const *_pError)
					{
						fprintf(stderr, "Error parsing limits when setting limits in launch helper: %s\n", _pError);
						return 1;
					}
				;

				while (*pParse)
				{
					if (*pParse != '<')
						return fReportParseError("Expected <");

					++pParse;
					auto pStart = pParse;
					fg_ParseNumeric(pParse);
					if (pStart == pParse)
						return fReportParseError("Expected rlimit number after <");
					if (pParse - pStart >= 16)
						return fReportParseError("RLimt number too large");

					int RLimit = fg_StrToInt(pStart, pParse - pStart);

					rlimit Limits;
					if (getrlimit(RLimit, &Limits))
					{
						fg_OutputError(errno, "The OS returned an error from getrlimit(%d) when setting process limits in launch helper: %s\n", RLimit);
						return 67;
					}

					while (true)
					{
						if (*pParse == '>')
						{
							++pParse;
							break;
						}
						if (*pParse != '=')
							return fReportParseError("Expected = after RLimit");

						++pParse;
						bool bMax = true;
						if (fg_StartsWith(pParse, "Max("))
							;
						else if (fg_StartsWith(pParse, "Cur("))
							bMax = false;
						else
							return fReportParseError("Expected Max( or Cur( after RLimit=");

						pParse += 4;

						auto pStart = pParse;
						fg_ParseNumeric(pParse);
						if (pStart == pParse)
							return fReportParseError("Expected limit inside ()");
						if (pParse - pStart >= 64)
							return fReportParseError("Limit too large");

						auto Limit = rlim_t(fg_StrToInt(pStart, pParse - pStart));
						if (bMax)
							Limits.rlim_max = Limit;
						else
							Limits.rlim_cur = Limit;

						if (*pParse != ')')
							return fReportParseError("Expected ) to terminate limit");

						++pParse;
					}

					if (setrlimit(RLimit, &Limits))
					{
						fg_OutputError
							(
								errno
								, "The OS returned an error from setrlimit(%d, {%lld, %lld}) when setting limits in launch helper: %s\n"
								, RLimit
								, Limits.rlim_cur
								, Limits.rlim_max
							)
						;
						return 67;
					}
				}
			}
			else
			{
				fprintf(stderr, "Unknown launch env var: %s", *pEnvironment);
				return 1;
			}

			++pEnvironment;
		}

		if (!bFoundEnd)
		{
			fprintf(stderr, "Expected environment to end with MalterlibLaunchEnd=true\n");
			return 1;
		}

		if (!pExecutableToLaunch)
		{
			fprintf(stderr, "Expected environment to start with MalterlibLaunch=true\n");
			return 1;
		}

		_Arguments[0] = (char *)pExecutableToLaunch;

		for (int i = 2; i < _nArgs; ++i)
			_Arguments[i - 1] = _Arguments[i];
		_Arguments[_nArgs - 1] = nullptr;

		execve(pExecutableToLaunch, _Arguments, pEnvironment);

		fg_OutputError(errno, "The OS returned an error from execve(%s) when executing in launch helper: %s\n", pExecutableToLaunch);

		return 1;
	}
	else if (strcmp(_Arguments[1], "--just-exit") == 0)
	{
		if (_nArgs > 2)
			return atoi(_Arguments[2]);
		return 0;
	}

	fprintf(stderr, "Invalid option: %s\n", _Arguments[1]);
	return 1;
}
