// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	if defined(DArchitecture_x86) || defined(DArchitecture_x64)
#		include <stdint.h>
#		include <x86intrin.h>
#	endif
#elif defined(DCompiler_MSVC)
#	ifndef _DDK_DRIVER_
#		include <intrin.h>
#	endif
#else
#	error "Implement this"
#endif

#if defined(DCompiler_MSVC) && defined(DArchitecture_arm64)
// ARM64

#define DMibHelperArm64SysReg(op0, op1, crn, crm, op2) \
        ( ((op0 & 1) << 14) | \
          ((op1 & 7) << 11) | \
          ((crn & 15) << 7) | \
          ((crm & 15) << 3) | \
          ((op2 & 7) << 0) )

#define DMibHelperArm64SysReg_OP1(_Reg_) (((_Reg_) >> 11) & 7)
#define DMibHelperArm64SysReg_CRN(_Reg_) (((_Reg_) >> 7) & 15)
#define DMibHelperArm64SysReg_CRM(_Reg_) (((_Reg_) >> 3) & 15)
#define DMibHelperArm64SysReg_OP2(_Reg_) ((_Reg_) & 7)

#define DMibArm64_CNTVCT_EL0        DMibHelperArm64SysReg(3,3,14, 0,2)  // Generic Timer counter register
#define DMibArm64_CNTFRQ_EL0        DMibHelperArm64SysReg(3,3,14, 0,0)  // Generic Timer counter frequency register
#define DMibArm64_PMCCNTR_EL0       DMibHelperArm64SysReg(3,3, 9,13,0)  // Cycle Count Register [CP15_PMCCNTR]
#define DMibArm64_PMSELR_EL0        DMibHelperArm64SysReg(3,3, 9,12,5)  // Event Counter Selection Register [CP15_PMSELR]
#define DMibArm64_PMXEVCNTR_EL0     DMibHelperArm64SysReg(3,3, 9,13,2)  // Event Count Register [CP15_PMXEVCNTR]
#define DMibArm64_PMXEVCNTRn_EL0(n) DMibHelperArm64SysReg(3,3,14, 8+((n)/8), (n)%8)    // Direct Event Count Register [n/a]
#define DMibArm64_TPIDR_EL0         DMibHelperArm64SysReg(3,3,13, 0,2)  // Thread ID Register, User Read/Write [CP15_TPIDRURW]
#define DMibArm64_TPIDRRO_EL0       DMibHelperArm64SysReg(3,3,13, 0,3)  // Thread ID Register, User Read Only [CP15_TPIDRURO]
#define DMibArm64_TPIDR_EL1         DMibHelperArm64SysReg(3,0,13, 0,4)  // Thread ID Register, Privileged Only [CP15_TPIDRPRW]

#endif


// Memory intrinsics
#if defined(DPlatformFamily_macOS)
	extern "C" void	*memcpy(void *, const void *, __SIZE_TYPE__);
	extern "C" int memcmp(const void *__s1, const void *__s2, size_t __n);
	extern "C" void	*memset(void *, int, __SIZE_TYPE__);
	extern "C" void	*memmove(void *, const void *, __SIZE_TYPE__);
#	define DMibPIntrinsicMemCopy(d_Dest, d_Source, d_Size) memcpy(d_Dest, d_Source, d_Size)
#	define DMibPIntrinsicMemCmp(d_Left, d_Right, d_Size) memcmp(d_Left, d_Right, d_Size)
#	define DMibPIntrinsicMemSet(d_Dest, d_Value, d_Size) memset(d_Dest, d_Value, d_Size)
#	define DMibPIntrinsicMemMove(d_Dest, d_Source, d_Size) memmove(d_Dest, d_Source, d_Size)
#elif defined(DPlatformFamily_Linux)
	extern "C" void *memcpy(void *__restrict __dest, __const void *__restrict __src, __SIZE_TYPE__ __n) throw () __attribute__ ((__nonnull__ (1, 2)));
	extern "C" int memcmp (__const void *__s1, __const void *__s2, size_t __n) throw () __attribute_pure__ __nonnull ((1, 2));
	extern "C" void *memset(void *__s, int __c, __SIZE_TYPE__ __n) throw () __attribute__ ((__nonnull__ (1)));
	extern "C" void *memmove(void *__dest, __const void *__src, __SIZE_TYPE__ __n) throw () __attribute__((__nonnull__ (1, 2)));
#	define DMibPIntrinsicMemCopy(d_Dest, d_Source, d_Size) memcpy(d_Dest, d_Source, d_Size)
#	define DMibPIntrinsicMemCmp(d_Left, d_Right, d_Size) memcmp(d_Left, d_Right, d_Size)
#	define DMibPIntrinsicMemSet(d_Dest, d_Value, d_Size) memset(d_Dest, d_Value, d_Size)
#	define DMibPIntrinsicMemMove(d_Dest, d_Source, d_Size) memmove(d_Dest, d_Source, d_Size)
#elif defined(DPlatformFamily_Windows)
	extern "C" void *  __cdecl memcpy(_Out_writes_bytes_all_(_Size) void * _Dst, _In_reads_bytes_(_Size) const void * _Src, _In_ size_t _Size);
	extern "C" int __cdecl memcmp(_In_reads_bytes_(_Size) void const* _Buf1, _In_reads_bytes_(_Size) void const* _Buf2, _In_ size_t _Size);
	extern "C" void *  __cdecl memmove(_Out_writes_bytes_all_opt_(_Size) void * _Dst, _In_reads_bytes_opt_(_Size) const void * _Src, _In_ size_t _Size);
	extern "C" _Post_equal_to_(_Dst) _At_buffer_((unsigned char*)_Dst, _Iter_, _Size, _Post_satisfies_(((unsigned char*)_Dst)[_Iter_] == _Val))
		void *  __cdecl memset(_Out_writes_bytes_all_(_Size) void * _Dst, _In_ int _Val, _In_ size_t _Size)
	;
#	pragma intrinsic(memcpy)
#	pragma intrinsic(memcmp)
#	pragma intrinsic(memset)
//#	pragma intrinsic(memmove)
#	define DMibPIntrinsicMemCopy(d_Dest, d_Source, d_Size) memcpy(d_Dest, d_Source, d_Size)
#	define DMibPIntrinsicMemCmp(d_Left, d_Right, d_Size) memcmp(d_Left, d_Right, d_Size)
#	define DMibPIntrinsicMemSet(d_Dest, d_Value, d_Size) memset(d_Dest, d_Value, d_Size)
#	define DMibPIntrinsicMemMove(d_Dest, d_Source, d_Size) memmove(d_Dest, d_Source, d_Size)
#else
#	error "Implement this"
#endif

// Prefetch
#if DArchitectureExtension_SSE
#	if defined(DCompiler_clang) || defined(DCompiler_gcc)
#		define DMibPPrefetch(d_ToPrefetch) _mm_prefetch((const char *)d_ToPrefetch, _MM_HINT_T0)
#		define DMibPPrefetchOneTimeUse(d_ToPrefetch) _mm_prefetch((const char *)d_ToPrefetch, _MM_HINT_NTA)
#	elif defined(DCompiler_MSVC)
#		pragma intrinsic(_mm_prefetch)
#		define DMibPPrefetch(d_ToPrefetch) _mm_prefetch((const char *)d_ToPrefetch, _MM_HINT_T0)
#		define DMibPPrefetchOneTimeUse(d_ToPrefetch) _mm_prefetch((const char *)d_ToPrefetch, _MM_HINT_NTA)
#	else
#		error "Implement this"
#	endif
#else
#	define DMibPPrefetch(d_ToPrefetch) (void)0
#	define DMibPPrefetchOneTimeUse(d_ToPrefetch) (void)0
#endif

// Yield
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	if defined(DArchitecture_x86) || defined(DArchitecture_x64)
#		define yield_cpu _mm_pause()
#	elif defined(DArchitecture_arm64) || defined(DArchitecture_arm64e)
#		define yield_cpu asm volatile("yield" ::: "memory")
#	else
#		error "Implement this"
#	endif
#elif defined(DCompiler_MSVC)
#	if defined(DArchitecture_x86) || defined(DArchitecture_x64)
#		define yield_cpu _mm_pause()
#	elif defined(DArchitecture_arm64) || defined(DArchitecture_arm64e)
#		define yield_cpu __yield()
#	else
#		error "Implement this"
#	endif
#else
#	error "Implement this"
#endif

// Highest bit set
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
	namespace NMib
	{
		namespace NPlatformHelpers
		{
			constexpr inline_always int fg_GetHighestBitSet32(unsigned long _Value)
			{
				if (!_Value)
					return -1;
				return 31 - __builtin_clz(_Value);
			}
			
			constexpr inline_always int fg_GetHighestBitSet32NoZero(unsigned long _Value)
			{
				return 31 - __builtin_clz(_Value);
			}
			
			constexpr inline_always int fg_GetLowestBitSet32(unsigned long _Value)
			{
				if (!_Value)
					return -1;
				return __builtin_ctz(_Value);
			}
			
			constexpr inline_always int fg_GetLowestBitSet32NoZero(unsigned long _Value)
			{
				return __builtin_ctz(_Value);
			}

#			if DMibPPtrBits >= 64
				constexpr inline_always int fg_GetHighestBitSet64(unsigned long _Value)
				{
					if (!_Value)
						return -1;
					return 63 - __builtin_clzll(_Value);
				}
				
				constexpr inline_always int fg_GetHighestBitSet64NoZero(unsigned long _Value)
				{
					return 63 - __builtin_clzll(_Value);
				}
				
				constexpr inline_always int fg_GetLowestBitSet64(unsigned long _Value)
				{
					if (!_Value)
						return -1;
					return __builtin_ctzll(_Value);
				}
				
				constexpr inline_always int fg_GetLowestBitSet64NoZero(unsigned long _Value)
				{
					return __builtin_ctzll(_Value);
				}
#			endif
		}
	}

#	define DMibPGetHighestBitSet32(d_Value) ::NMib::NPlatformHelpers::fg_GetHighestBitSet32(d_Value)
#	define DMibPGetHighestBitSet32NoZero(d_Value) ::NMib::NPlatformHelpers::fg_GetHighestBitSet32NoZero(d_Value)
#	define DMibPGetLowestBitSet32(d_Value) ::NMib::NPlatformHelpers::fg_GetLowestBitSet32(d_Value)
#	define DMibPGetLowestBitSet32NoZero(d_Value) ::NMib::NPlatformHelpers::fg_GetLowestBitSet32NoZero(d_Value)
#	if DMibPPtrBits >= 64
	#	define DMibPGetHighestBitSet64(d_Value) ::NMib::NPlatformHelpers::fg_GetHighestBitSet64(d_Value)
	#	define DMibPGetHighestBitSet64NoZero(d_Value) ::NMib::NPlatformHelpers::fg_GetHighestBitSet64NoZero(d_Value)
	#	define DMibPGetLowestBitSet64(d_Value) ::NMib::NPlatformHelpers::fg_GetLowestBitSet64(d_Value)
	#	define DMibPGetLowestBitSet64NoZero(d_Value) ::NMib::NPlatformHelpers::fg_GetLowestBitSet64NoZero(d_Value)
#	endif

#elif defined(DCompiler_MSVC)

#	pragma intrinsic(_BitScanReverse)
#	if DMibPPtrBits >= 64
#		pragma intrinsic(_BitScanReverse64)
#	endif
	namespace NMib
	{
		namespace NPlatformHelpers
		{
			inline_always int fg_GetHighestBitSet32(unsigned long _Value)
			{
				if (!_Value)
					return -1;
				unsigned long Ret;
				_BitScanReverse(&Ret, _Value);
				return Ret;
			}

			inline_always int fg_GetHighestBitSet32NoZero(unsigned long _Value)
			{
				unsigned long Ret;
				_BitScanReverse(&Ret, _Value);
				return Ret;
			}

			inline_always int fg_GetLowestBitSet32(unsigned long _Value)
			{
				if (!_Value)
					return -1;
				unsigned long Ret;
				_BitScanForward(&Ret, _Value);
				return Ret;
			}

			inline_always int fg_GetLowestBitSet32NoZero(unsigned long _Value)
			{
				unsigned long Ret;
				_BitScanForward(&Ret, _Value);
				return Ret;
			}
#			if DMibPPtrBits >= 64
				inline_always int fg_GetHighestBitSet64(unsigned __int64 _Value)
				{
					if (!_Value)
						return -1;
					unsigned long Ret;
					_BitScanReverse64(&Ret, _Value);
					return Ret;
				}

				inline_always int fg_GetHighestBitSet64NoZero(unsigned __int64 _Value)
				{
					unsigned long Ret;
					_BitScanReverse64(&Ret, _Value);
					return Ret;
				}

				inline_always int fg_GetLowestBitSet64(unsigned __int64 _Value)
				{
					if (!_Value)
						return -1;
					unsigned long Ret;
					_BitScanForward64(&Ret, _Value);
					return Ret;
				}

				inline_always int fg_GetLowestBitSet64NoZero(unsigned __int64 _Value)
				{
					unsigned long Ret;
					_BitScanForward64(&Ret, _Value);
					return Ret;
				}
#			endif
		}
	}

#	define DMibPGetHighestBitSet32(d_Value) NMib::NPlatformHelpers::fg_GetHighestBitSet32(d_Value)
#	define DMibPGetHighestBitSet32NoZero(d_Value) NMib::NPlatformHelpers::fg_GetHighestBitSet32NoZero(d_Value)
#	define DMibPGetLowestBitSet32(d_Value) NMib::NPlatformHelpers::fg_GetLowestBitSet32(d_Value)
#	define DMibPGetLowestBitSet32NoZero(d_Value) NMib::NPlatformHelpers::fg_GetLowestBitSet32NoZero(d_Value)

#	if DMibPPtrBits >= 64
#		define DMibPGetHighestBitSet64(value) NMib::NPlatformHelpers::fg_GetHighestBitSet64(value);
#		define DMibPGetHighestBitSet64NoZero(value) NMib::NPlatformHelpers::fg_GetHighestBitSet64NoZero(value);
#		define DMibPGetLowestBitSet64(value) NMib::NPlatformHelpers::fg_GetLowestBitSet64(value);
#		define DMibPGetLowestBitSet64NoZero(value) NMib::NPlatformHelpers::fg_GetLowestBitSet64NoZero(value);
#	endif

#else
#	error "Implement this"
#endif

// Num bits set
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	ifndef DConfig_NoNewInstructions
#		define DMibPNumBitsSet32(d_Value) __builtin_popcount(d_Value)
#		if DMibPPtrBits >= 64
#			define DMibPNumBitsSet64(d_Value) __builtin_popcountll(d_Value)
#		endif
#	endif
#elif defined(DCompiler_MSVC)
#	if defined(DArchitecture_arm64)
#		define DMibPNumBitsSet32(d_Value) _CountOneBits(d_Value)
#		define DMibPNumBitsSet64(d_Value) _CountOneBits64(d_Value)
#	else
#		ifndef DConfig_NoNewInstructions
#			if defined(DArchitecture_x86) || defined(DArchitecture_x64)
#				define DMibPNumBitsSet32(d_Value) __popcnt(d_Value)
#				if defined(DArchitecture_x64)
#					define DMibPNumBitsSet64(_x) __popcnt64(_x)
#				endif
#			else
#				error "Implement this"
#			endif
#		endif
#	endif
#else
#	error "Implement this"
#endif


// Byte swap
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	define DMibPByteSwap16(d_Value) __builtin_bswap16(d_Value);
#	define DMibPByteSwap32(d_Value) __builtin_bswap32(d_Value);
#	define DMibPByteSwap64(d_Value) __builtin_bswap64(d_Value);
#elif defined(DCompiler_MSVC)
#	pragma intrinsic(_byteswap_ushort)
#	pragma intrinsic(_byteswap_ulong)
#	pragma intrinsic(_byteswap_uint64)
#	define DMibPByteSwap16(d_Value) _byteswap_ushort(d_Value)
#	define DMibPByteSwap32(d_Value) _byteswap_ulong(d_Value)
#	define DMibPByteSwap64(d_Value) _byteswap_uint64(d_Value)
#else
#	error "Implement this"
#endif


// Rotate
#if defined(DCompiler_clang) || defined(DCompiler_gcc)

#elif defined(DCompiler_MSVC)

#	pragma intrinsic(_rotl8)
#	pragma intrinsic(_rotr8)
#	pragma intrinsic(_rotl16)
#	pragma intrinsic(_rotr16)

#	define DMibPRotateLeft8(d_Value, d_Shift) _rotl8(d_Value, d_Shift)
#	define DMibPRotateLeft16(d_Value, d_Shift) _rotl16(d_Value, d_Shift)
#	define DMibPRotateLeft32(d_Value, d_Shift) _rotl(d_Value, d_Shift)
#	define DMibPRotateLeft64(d_Value, d_Shift) _rotl64(d_Value, d_Shift)

#	pragma intrinsic(_rotl)
#	pragma intrinsic(_rotr)
#	pragma intrinsic(_rotl64)
#	pragma intrinsic(_rotr64)

#	define DMibPRotateRight8(d_Value, d_Shift) _rotr8(d_Value, d_Shift)
#	define DMibPRotateRight16(d_Value, d_Shift) _rotr16(d_Value, d_Shift)
#	define DMibPRotateRight32(d_Value, d_Shift) _rotr(d_Value, d_Shift)
#	define DMibPRotateRight64(d_Value, d_Shift) _rotr64(d_Value, d_Shift)

#else
#	error "Implement this"
#endif



// Debug break
#if defined(DCompiler_clang) || defined(DCompiler_gcc)
#	if DPlatformFamily_Emscripten
#		define DMibPDebugBreak EM_ASM(debugger();)
#	elif defined(DArchitecture_x64) || defined(DArchitecture_x86)
#		define DMibPDebugBreak __asm__ ("int $3")
#	else
#		define DMibPDebugBreak __builtin_debugtrap()
#	endif
#elif defined(DCompiler_MSVC)
#	pragma intrinsic(__debugbreak)
#	define DMibPDebugBreak __debugbreak()
#else
#	error "Implement this"
#endif

