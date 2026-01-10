# CLAUDE.md - Malterlib Framework and Core Module

This file provides comprehensive guidance to Claude Code (claude.ai/code) when working with the Malterlib framework. Since the Core module is the foundation of the entire Malterlib framework, this document contains both general framework guidelines and Core module-specific information.

## Malterlib Framework Overview

Malterlib is a comprehensive C++ framework and build system that provides cross-platform development tools, libraries, and applications. The project uses a custom build system called MTool/mib (Malterlib Build) with its own DSL for describing build configurations.

## Build System

### Core Build Tool
- **mib** - The main build command located at `./mib` (shell script wrapper)
- Uses custom `.MHeader`, `.MTarget`, `.MBuildSystem`, `.MRepo` files for configuration
- Supports multiple platforms: macOS, Windows, Linux
- Supports multiple architectures: arm64, x86, x86

### Common Build Commands

```bash
# Generate build system for a workspace
./mib generate [WorkspaceName]

# Build a workspace
MalterlibBuildShowProgress=false ./mib build [WorkspaceName] [Platform] [Architecture] [Configuration]
# Example: ./mib build Tests macOS arm64 Debug

# Build a specific target within a workspace
MalterlibBuildShowProgress=false ./mib build_target [WorkspaceName] [TargetName] [Platform] [Architecture] [Configuration]
# Example: ./mib build Tests Com_Test_Malterlib_Container macOS arm64 Debug

# Build and run tests
./mib test [Configuration]  # Default is Debug
./mib test_release          # Run tests with Release configuration

# Update repositories
./mib update_repos

# Get repository status
./mib status

# Setup prerequisites (macOS only)
./mib setup

# Run git commands across all repositories
./mib git [GitCommand] [Parameters]

# Checkout a branch across all repositories
./mib branch [BranchName]

# Push changes across all repositories
./mib push

# Clean up branches that have been pushed
./mib cleanup-branches [BranchNames...]

# Run a specific test
/opt/Deploy/Tests/RunAllTests --paths '["Path/To/Test", "Path/To/Test2", "Path/To/Test*"]' # macOS
/Deploy/Tests/RunAllTests --paths '["Path/To/Test", "Path/To/Test2", "Path/To/Test*"]' # Linux
/c/Tests/RunAllTests --paths '["Path/To/Test", "Path/To/Test2", "Path/To/Test*"]' # Windows

# Run tests with quiet output (only show failures)
/opt/Deploy/Tests/RunAllTests --quiet

# Get help for mib commands
./mib --help
./mib --help [CommandName]  # Detailed help for specific command
```

Remember to use MalterlibBuildShowProgress=false when building so you don't get overwhelmed with uncessary output.

### Build Configurations
- **Debug** - Debug build with assertions and debug symbols
- **Release** - Optimized release build
- **Release (Tests)** - Release build with test support

### Build Platforms
- **Windows** - Buildable on Windows host
- **macOS** - Buildable on macOS host
- **Linux** - Buildable on macOS host

### Build Configurations
- **arm64** - Supported on macOS, Linux and Windows
- **x64** - Supported on macOS, Linux and Windows
- **x86** - Only supported on Linux

## Framework Architecture

### Module Organization
The codebase is organized into modules under the `Malterlib/` directory. Each module may contain its own CLAUDE.md file with module-specific guidance - see the [Related Module Documentation](#related-module-documentation) section below for links.

### External Dependencies
Located in `External/` directory:
- boost
- CMake
- curl
- libarchive
- LLVM
- MariaDB connector
- MongoDB drivers
- nginx
- Node.js
- Qt
- SQLite
- zlib

### Build System Files
- **.MBuildSystem** - Root build system configuration
- **.MHeader** - Module header files defining targets and properties
- **.MTarget** - Target definitions
- **.MRepo** - Repository configuration
- **.MSettings** - Settings files

## Development Workflow

### Adding New Code
1. Place code in appropriate module directory under `Malterlib/`
2. Update or create `.MHeader` files to include new targets
3. Run `./mib generate` to regenerate build files
4. Build with `MalterlibBuildShowProgress=false ./mib build [workspace]`

### Running Tests
1. Generate test workspace: `./mib generate Tests`
2. Build tests: `MalterlibBuildShowProgress=false ./mib build Tests [Platform] [Architecture] Debug`
3. Run tests: `./mib test` or directly execute `RunAllTests` binary
4. To run specific tests: `/opt/Deploy/Tests/RunAllTests --paths '["Module/Test/Name"]'`
5. For continuous testing during development: build with Debug configuration for faster iteration

### Repository Management
- Check status: `./mib status`
- Update all repos: `./mib update_repos`
- Switch branch: `./mib branch [BranchName]`
- Push changes: `./mib push`
- The system uses git LFS for binary dependencies - ensure it's installed
- Many directories under `Malterlib/` are separate Git repositories. When checking status or searching history, run Git commands inside the relevant subdirectory (`Malterlib/Concurrency`, `Malterlib/Cloud`, etc.) or use the helper scripts (`./mib git ...`) that fan out across sub-repos.

### Working with Workspaces
Workspaces are collections of build targets. Common workspaces include:
- **Tests** - All test targets
- **MTool** - Build system tools
- **Malterlib_[Module]** - Specific module workspaces
- **Apps_Malterlib_[Module]** - Application workspaces

Generate a workspace before building: `./mib generate [WorkspaceName]`

## Code Standards

### Formatting Rules

#### Indentation
- Use 1 tab character for indentation (tab width = 4 columns)
- Do not expand tabs to spaces
- Tabs are preferred for easier navigation
- **All files in `Malterlib/` use tabs** - when editing, always assume that the file has tabs, unless you know different
- Files in `External/` (third-party code) may use different conventions - check the specific file

#### Line Length
- Maximum 190 columns
- Designed to fit on screens down to 1280x720 resolution
- Allows side-by-side code viewing on wider screens

#### Whitespace and Operators
- Most operators have spaces before and after (except `.`, `->`, `*`)
- Comma operator has a space after, but not before
- Example: `int a = b * c;`

#### Braces and Blocks
- Opening brace `{` starts on a new line under the associated keyword
- Closing brace `}` is at the same indent level as the start
- Single statements do not require braces
- Space between keyword and parenthesis

#### Statement Splitting
- Each substatement on the same logical level goes on its own line
- Scope markers must be on separate lines
- Complex statements should be broken down for readability

### Naming Conventions

#### General Rules
- Use Upper Camel Case for most names
- Exceptions: Language functionality emulation uses lower case (e.g., `inline_always`, `uint32`)

#### Function Prefixes
- `f_`: Member function
- `fp_`: Private/protected member function
- `fs_`: Static member function
- `fsp_`: Private/protected static member function
- `fg_`: Global function
- `fsg_`: Static global function

#### Parameter Prefixes
- `_`: Standard function parameter
- `p_`: Function parameter pack
- `o_`: Output parameter
- `po_`: Output parameter pack
- `t_`: Template parameter
- `tp_`: Template parameter pack
- `d_`: Macro parameter

#### Variable Prefixes
- No prefix for local variables
- `c_`: Compile-time constant local variables
- `s_`: Static local variables
- `g_`: Global variables
- `gc_`: Compile-time constant global variables
- `gs_`: Static global variables
- `m_`: Member variables
- `mc_`: Compile-time constant member variables
- `ms_`: Static member variables
- `mp_`: Private/protected member variables

#### Type Prefixes
- `E`: Enums and enumerators
- `N`: Namespaces
- `C`: Classes, structs, typedefs
- `IC`: Interface classes
- `F`: Function types
- `TC`: Template classes
- `TIC`: Template interface classes
- `c`: Concepts

#### Conceptual Prefixes (after storage prefix)
- `b`: Boolean
- `i`: Iterator or index
- `n`: Number of
- `p`: Pointer
- `f`: Function object
- `r`: Range

## Important Framework Notes

- The build system uses absolute paths by default
- Build artifacts are placed in `/opt/Deploy/`, `/Deploy/` or `/c/Deploy/` depending on the OS and `BuildSystem/Default/PostCopy.MConfig`
- The system supports cross-compilation for multiple platforms
- Use `./mib --help` for detailed command information
- The project uses custom memory management with configurable allocators
- LFS (Large File Storage) is used for binary dependencies
- The build system caches environment and dependency information in `BuildSystem/Default/`
- When switching branches, run `./mib update_repos` to ensure all repositories are synchronized
- The mib script automatically bootstraps required tools on first use

## Core Module Overview

The Core module provides:
- **Platform Abstraction Layer** - Cross-platform support for macOS, Windows, Linux
- **Build System Components** - mib (Malterlib Build) system configuration and generators
- **Application Framework** - Base application class and entry point management
- **Subsystem Management** - Lazy initialization and lifecycle management
- **Runtime Type System** - RTTI and dynamic type identification
- **Platform Detection** - Compile-time and runtime platform detection
- **Core Utilities** - Scope guards, enum operators, type traits

## Directory Structure

```
Core/
├── Build/               # Build system configurations
│   ├── Clang/          # Clang-specific settings
│   ├── VisualStudio/   # Visual Studio generators
│   └── Xcode/          # Xcode generators
├── BuildScripts/       # Platform-specific build scripts
├── Documentation/      # Module documentation
├── Export/             # Export configurations per language
├── Include/Mib/Core/   # Public headers
├── Source/             # Implementation files
│   └── Platform/       # Platform-specific implementations
├── Test/               # Module tests
└── Tools/              # Core tools and utilities
```

## Key Components

### Application Framework

The application framework provides the main entry point abstraction:

```cpp
// Application implementation example
namespace NAppName
{
	class CMyApp : public NMib::CApplication
	{
	public:
		virtual aint f_Main() override
		{
			// Application logic here
			return 0;
		}
	};
}

// Register the application
DAppImplement(CMyApp)
```

### Subsystem Management

Subsystems provide lazy initialization with guaranteed destruction order useful for functionality that should outlive the main function:

```cpp
// Define a subsystem
class CMySubSystem
{
public:
	void f_Initialize()
	{
		// Initialization code
	}

	void f_DoWork()
	{
		// Subsystem functionality
	}
};

// Declare the subsystem with destruction order
namespace NMib
{
	extern TCSubSystem<CMySubSystem, ESubSystemDestruction::EAfterMain> g_MySubSystem;
}

// Use the subsystem (lazy initialized on first access)
void fg_UseSubSystem()
{
	NMib::g_MySubSystem->f_DoWork();
}
```

### Platform Detection

Platform detection happens at compile-time through macros:

```cpp
// Platform family detection
#ifdef DMibPlatformFamily_macOS
	// macOS-specific code
#elif DMibPlatformFamily_Windows
	// Windows-specific code
#elif DMibPlatformFamily_Linux
	// Linux-specific code
#endif

// Architecture detection
#ifdef DMibArchitecture_x64
	// x86_64-specific code
#elif DMibArchitecture_arm64
	// ARM64-specific code
#endif

// Compiler detection
#ifdef DMibCompiler_MSVC
	// MSVC-specific code
#elif DMibCompiler_Clang
	// Clang-specific code
#endif
```

### Scope Guards

RAII-based scope guards for cleanup:

```cpp
void fg_Example()
{
	FILE *pFile = fopen("test.txt", "r");

	// Ensure file is closed on scope exit
	auto Cleanup = NMib::g_OnScopeExit / [&]
		{
			if (pFile)
				fclose(pFile);
		}
	;

	// Use file...
	// File automatically closed when scope exits
}

// Conditional cleanup - can clear the scope guard
void fg_ConditionalCleanup()
{
	bool bSuccess = false;
	auto Cleanup = NMib::g_OnScopeExit / [&]
		{
			if (!bSuccess)
				f_PerformRollback();
		}
	;

	// Do work...
	if (fg_OperationSucceeded())
	{
		bSuccess = true;
		Cleanup.f_Clear();  // Cancel the cleanup
	}
}

// Exception-safe cleanup
void fg_ExceptionSafeCleanup()
{
	auto Cleanup = NMib::g_OnScopeExitCatch / [&]
		{
			// This runs even if an exception is thrown
			// and catches any exceptions from the cleanup itself
			f_PerformCleanup();
		}
	;

	// Do work that might throw...
}
```

### Runtime Type System

Runtime type identification and class registration:

```cpp
// Define a runtime class
class CMyClass : public NMib::CRuntimeClass
{
	DMibRuntimeClass(NMib::CRuntimeClass, CMyClass);

public:
	void f_DoSomething()
	{
		// Implementation
	}
};

// Use runtime type checking
void fg_ProcessObject(NMib::CRuntimeClass *_pObject)
{
	if (CMyClass *pMyClass = DMibDynamicCast<CMyClass>(_pObject))
		pMyClass->f_DoSomething();
}
```

### Enum Operators

Type-safe enum class operators:

```cpp
// Define an enum with operators
enum class EMyFlags : uint32
{
	ENone = 0,
	EFlag1 = 1 << 0,
	EFlag2 = 1 << 1,
	EFlag3 = 1 << 2
};
DMibEnumOperators(EMyFlags);

// Use enum operators
void fg_UseFlags()
{
	EMyFlags nFlags = EMyFlags::EFlag1 | EMyFlags::EFlag2;

	if (nFlags & EMyFlags::EFlag1)
	{
		// Flag1 is set
		fg_ProcessFlag1();
	}

	nFlags &= ~EMyFlags::EFlag2;  // Clear Flag2
}
```

## Platform-Specific Implementation

### File Organization

Platform-specific code uses different file extensions for different purposes:

**Implementation Files (.cpp)**
- `Malterlib_Core_PlatformImp_MSVC.cpp` - Windows/MSVC implementation
- `Malterlib_Core_PlatformImp_MacOS.cpp` - macOS implementation
- `Malterlib_Core_PlatformImp_Linux.cpp` - Linux implementation
- `Malterlib_Core_PlatformImp_Posix_Init.cpp` - Shared POSIX initialization
- `Malterlib_Core_Platform_Windows_*.cpp` - Windows-specific features
- `Malterlib_Core_Platform_MacOS_*.cpp` - macOS-specific features
- `Malterlib_Core_Platform_POSIX_*.cpp` - POSIX-specific features

**Implementation Headers (.imp.h)**
- `Malterlib_Core_PlatformImp_POSIX.imp.h` - POSIX implementation templates
- `Malterlib_Core_PlatformImp_*_Net.imp.h` - Network implementations
- `Malterlib_Core_PlatformImp.imp.h` - Core platform implementation

**Header Templates (.hpp)**
- `Malterlib_Core_PlatformImp_POSIX_*.hpp` - POSIX template implementations
- `Malterlib_Core_Platform_*_*.hpp` - Platform-specific templates

## Build System Integration

### MHeader Files

The Core module uses several .MHeader files:
- `Malterlib_Core.MHeader` - Main module configuration
- `Malterlib_Core_Modules.MHeader` - Module dependencies
- `Malterlib_Core_boost.MHeader` - Boost integration
- `Malterlib_Core_Export.MHeader` - Export settings

### Build Configurations

Build settings are organized hierarchically:
- `Shared.MSettings` - Base settings for all platforms
- `Shared_Compile.MSettings` - Compilation settings
- `Shared_Dependencies.MSettings` - Dependency management
- `Shared_Target.MSettings` - Target configuration

### Generator Settings

Platform-specific generators:
- `VisualStudio.MGeneratorSettings` - Visual Studio project generation
- `Xcode.MGeneratorSettings` - Xcode project generation
- `Clang.MGeneratorSettings` - Clang compilation settings

## Dependencies

### Internal Dependencies

Core has minimal internal dependencies:
- **Memory** - Custom memory allocators (when MalterlibSubLibrarySeparate)
- **Thread** - Threading primitives (when MalterlibSubLibrarySeparate)
- **Atomic** - Atomic operations (when MalterlibSubLibrarySeparate)
- **String** - String utilities (when MalterlibSubLibrarySeparate)

### External Dependencies

Platform-specific system libraries:
- **macOS**: CoreFoundation, Security, AppKit, Cocoa, IOKit
- **Windows**: kernel32, user32, advapi32, shell32
- **Linux**: pthread, dl, rt

## Code Examples

### Creating a Simple Application

```cpp
// MyApp.cpp
#include <Mib/Core/Application>
#include <Mib/String/Str>

namespace NAppName
{
	class CMyApp : public NMib::CApplication
	{
	public:
		virtual aint f_Main() override
		{
			// Get command line parameters
			aint nNumParams = f_NumCommandLineParameters();
			for (aint i = 0; i < nNumParams; ++i)
			{
				NStr::CStr Param = f_CommandLineParameter(i);
				// Process parameter
			}

			// Application logic
			return 0;  // Success
		}
	};
}

// Register the application
DAppImplement(CMyApp)
```

### Platform-Specific Code

```cpp
// CrossPlatformFile.cpp
#include <Mib/Core/Platform>

void fg_CreateConfigDirectory()
{
	NStr::CStr Path;

#ifdef DMibPlatformFamily_Windows
	Path = NPlatform::fg_GetEnvironmentVariable("APPDATA");
	Path += "\\MyApp";
#elif DMibPlatformFamily_macOS
	Path = NPlatform::fg_GetHomeDirectory();
	Path += "/Library/Application Support/MyApp";
#elif DMibPlatformFamily_Linux
	Path = NPlatform::fg_GetHomeDirectory();
	Path += "/.config/myapp";
#endif

	if (!NPlatform::fg_DirectoryExists(Path))
		NPlatform::fg_CreateDirectory(Path);
}
```

### Using Subsystems

```cpp
// LoggingSubSystem.cpp
#include <Mib/Core/SubSystem>

class CLoggingSubSystem
{
public:
	CLoggingSubSystem()
	{
		// Constructor - called on first access
		fp_Initialize();
	}

	~CLoggingSubSystem()
	{
		// Destructor - called at program exit
		fp_Shutdown();
	}

	void f_Log(NStr::CStr const &_Message)
	{
		// Log message
	}

private:
	void fp_Initialize()
	{
		// Open log file, etc.
	}

	void fp_Shutdown()
	{
		// Close log file, flush buffers
	}
};

// Define global subsystem
namespace NMib
{
	constinit TCSubSystem<CLoggingSubSystem, ESubSystemDestruction::EBeforeMain> g_Logging;
}

// Use the subsystem
void fg_LogMessage(NStr::CStr const &_Message)
{
	NMib::g_Logging->f_Log(_Message);
}
```

## Testing

Core module tests are located in `Test/` directory:
- `Test_Malterlib_Core_Operators.cpp` - Operator overloading tests
- `Test_Malterlib_Core_Move.cpp` - Move semantics tests
- `Test_Malterlib_Core_StdLib.cpp` - Standard library compatibility
- `Test_Malterlib_Core_CodeFormatting.cpp` - Code formatting validation
- `Test_Malterlib_Core_CodeColoring.cpp` - Syntax highlighting tests

Run tests with:
```bash
/opt/Deploy/Tests/RunAllTests --paths '["Malterlib/Core/*"]'
# Or specific test
/opt/Deploy/Tests/RunAllTests --paths '["Malterlib/Core/Test/Operators"]'
```

## Important Notes

### Thread Safety
- Subsystems use spin locks for thread-safe lazy initialization
- Platform functions are generally thread-safe unless noted
- Application class is single-instance, not thread-safe

### Memory Management
- Core does not allocate heap memory during static initialization
- Subsystems are constructed with placement new with memory from the image
- Platform functions may allocate memory as needed

### Error Handling
- Platform functions typically return bool for success/failure
- Critical failures use exceptions

### Performance Considerations
- Subsystem access has one-time initialization cost
- Platform detection is compile-time (zero runtime cost)
- Scope guards have minimal overhead (optimized away in release)

## Common Patterns

### Singleton Pattern via Subsystems
```cpp
class CMySingleton
{
public:
	static CMySingleton &fs_Get()
	{
		return *NMib::g_MySingleton;
	}

	void f_DoWork();
};

namespace NMib
{
	constinit TCSubSystem<CMySingleton, ESubSystemDestruction::EAfterMain> g_MySingleton;
}
```

## Debugging Support

### Assertions
```cpp
// Debug assertions (removed in release)
DMibCheck(_pPointer != nullptr);
```

### Platform-Specific Debugging
```cpp
#ifdef DMibDebug
	// Debug-only code
	NPlatform::fg_DebugBreak();  // Trigger debugger
	NPlatform::fg_OutputDebugString("Debug message");
#endif
```

## Best Practices

1. **Always use platform abstraction** - Never use OS APIs directly
2. **Prefer subsystems over globals** - Use TCSubSystem for singletons
3. **Use scope guards for cleanup** - `g_OnScopeExit / [&]{}` for RAII
4. **Check platform at compile-time** - Use DMibPlatformFamily_* macros
5. **Follow naming conventions** - See main CLAUDE.md for standards
6. **Test on all platforms** - Core changes affect entire framework
7. **Document platform differences** - Note any platform-specific behavior
8. **Minimize dependencies** - Core should have minimal external dependencies

## Common Development Debugging Mistakes

Based on real development sessions, here are common mistakes to avoid when debugging Malterlib issues:

1. **Fix symptoms instead of root causes** - When functionality isn't working, analyze the underlying implementation rather than creating workarounds in application code.

2. **Add logging to wrong execution paths** - Use comprehensive diagnostic logging to understand which code paths are actually being taken before making assumptions about program flow.

3. **Treat all error conditions the same** - Different error codes often have different meanings. Only handle specific error conditions you can prove are the intended case, and report others as distinct error types.

4. **Use human-readable error messages** - Always use platform error translation functions (like `NPlatform::fg_Win32_GetLastErrorStr()`) instead of just showing numeric error codes to users or logs.

5. **Don't fix multiple C++ compilation errors simultaneously** - In C++, always fix the first compilation error first. Other errors often cascade from the initial issue and may resolve automatically once the first error is fixed. Attempting to fix multiple errors simultaneously can lead to confusion and wasted effort.

6. **Distinguish IDE diagnostics from manual build diagnostics** - When using the `mcp__ide__getDiagnostics` tool, diagnostics come from different sources:
   - **clangd diagnostics** (language server): Have `"source": "clang"` and `"code"` fields (e.g., `"code": "ovl_no_viable_oper"`). These are live and updated as you edit files - trust these for current file state. Also note that you cannot always trust the diagnostics in header files, because clangd doesn't always have the context correct.
   - **Manual build diagnostics**: Have NO `source` or `code` fields. These are stale results from a previous build the user ran manually and may not reflect current code state.

   **Best practice**: Ignore diagnostics without a `source` field unless the user explicitly says something like "check the last build I did" or "look at the build output". When in doubt, trust clangd diagnostics (those with `"source": "clang"`) as they reflect the current state of the code.

## Module-Specific Coding Standards

In addition to the general Malterlib coding standards:

1. **Platform code organization** - Platform-specific implementations go in Source/Platform/
2. **Export configurations** - Language-specific exports go in Export/
3. **Build system files** - Keep build configurations in Build/ directory
4. **Include guards** - Use `#pragma once` for all headers
5. **Inline directives** - Use `inline_always`, `inline_never` for optimization hints
6. **Debug markers** - Use `mark_nodebug` to exclude from debug builds

## Related Module Documentation

Since Core is the foundation of Malterlib, here are references to all module-specific CLAUDE.md files that provide detailed guidance for each module:

### Framework Modules
- **Container**: `@../Container/CLAUDE.md` - Data structures and containers
- **Concurrency**: `@../Concurrency/CLAUDE.md` - Async operations and parallel processing
- **Encoding**: `@../Encoding/CLAUDE.md` - Text encoding and character sets
- **Storage**: `@../Storage/CLAUDE.md` - Persistent storage abstractions
- **String**: `@../String/CLAUDE.md` - String operations and formatting

- **Atomic**: `../Atomic/CLAUDE.md` - Atomic operations and lock-free programming
- **BuildSystem**: `../BuildSystem/CLAUDE.md` - Build system components and mib tool
- **Cloud**: `../Cloud/CLAUDE.md` - Cloud services integration
- **Cryptography**: `../Cryptography/CLAUDE.md` - Cryptographic operations
- **Database**: `../Database/CLAUDE.md` - Database interfaces and abstractions
- **File**: `../File/CLAUDE.md` - File system operations
- **Function**: `../Function/CLAUDE.md` - Function utilities and delegates
- **Intrusive**: `../Intrusive/CLAUDE.md` - Intrusive data structures
- **Network**: `../Network/CLAUDE.md` - Networking and communication
- **Numeric**: `../Numeric/CLAUDE.md` - Numerical operations and math
- **Process**: `../Process/CLAUDE.md` - Process management and IPC
- **Stream**: `../Stream/CLAUDE.md` - Stream processing and I/O
- **Time**: `../Time/CLAUDE.md` - Time and date utilities
- **Web**: `../Web/CLAUDE.md` - Web server and HTTP handling


### Main Framework Documentation
- **Project Overview**: `../../CLAUDE.md` - Main project guidelines and standards

When working with any of these modules, refer to their specific CLAUDE.md files for module-specific patterns, best practices, and implementation details.

### Important Modules that should always be in context
@../String/CLAUDE.md
@../Storage/CLAUDE.md
@../Container/CLAUDE.md
@../Encoding/CLAUDE.md
@../Concurrency/CLAUDE.md
