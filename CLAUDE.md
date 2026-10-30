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

### Shell Setup

Before running any commands, enable pipefail so that piped commands propagate failure exit codes correctly:

```bash
set -o pipefail
```

Without this, piping output through `tail`, `head`, etc. will hide non-zero exit codes from the original command.

### Common Build Commands

```bash
# Generate build system for a workspace
./mib generate [WorkspaceName]

# Generate and build a workspace
MalterlibBuildShowProgress=false ./mib build WorkspaceName [Platform] [Architecture] [Configuration]
# Example: ./mib build Tests

# Generate and build one or more targets within a workspace, in one build (several targets are comma separated)
MalterlibBuildShowProgress=false ./mib build-target WorkspaceName TargetName[,TargetName...] [Platform] [Architecture] [Configuration]
# Example: ./mib build-target Tests Com_Test_Malterlib_Container,Com_Test_Malterlib_String

# Generate, build and run tests
MalterlibBuildShowProgress=false ./mib test

# Update repositories
./mib update-repos

# Get repository status
./mib status

# Regenerate AGENTS.md from CLAUDE.md and its Markdown includes
./mib update-agents

# Setup prerequisites (macOS only)
./mib setup

# Run git commands across all repositories
./mib git [GitCommand] [Parameters]

# Checkout a branch across all repositories
./mib branch [BranchName]

# Push changes across all repositories
./mib push

# Run a specific test
/opt/Deploy/Tests/RunAllTests --paths '["Path/To/Test", "Path/To/Test2", "Path/To/Test*"]' # macOS
/Deploy/Tests/RunAllTests --paths '["Path/To/Test", "Path/To/Test2", "Path/To/Test*"]' # Linux
/c/Tests/RunAllTests --paths '["Path/To/Test", "Path/To/Test2", "Path/To/Test*"]' # Windows

# Run tests without quiet output
/opt/Deploy/Tests/RunAllTests --no-quiet

# Get help for mib commands
./mib --help
./mib --help [CommandName]  # Detailed help for specific command
```

Remember to use MalterlibBuildShowProgress=false when building so you don't get overwhelmed with uncessary output.

Never invoke `ninja` directly, and never run two builds of the same configuration at the same time. Only one build may run in a configuration's build directory at once: ninja locks the build directory for the whole build and refuses to start while another ninja holds it, because two ninja processes in one build directory corrupt its build and dependency logs and the recovery then rebuilds everything. To build several targets, pass them comma separated to one `./mib build-target` command instead of starting parallel builds.

To change the platform to target edit BuildSystem/Default/UserSettings.MSettings and change (and uncomment if needed) SingleArchitecture, SingleConfiguration, SinglePlatform

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
3. Generate and build with `MalterlibBuildShowProgress=false ./mib build [workspace]`

### Running Tests

Always read `Malterlib/Test/CLAUDE.md` before writing or modifying tests in any module.

1. Build and run tests: `./mib test`
2. To run specific tests: `/opt/Deploy/Tests/RunAllTests --paths '["Module/Test/Name"]'`
3. Build and run specific tests: `./mib test --paths '["Module/Test/Name"]'`
4. **Checking test results**: Test success is determined solely by exit code 0. Do not try to grep or parse the test output for PASS/FAIL strings — the Bash tool already reports the exit code directly. If the command exits with 0, all tests passed.

### Repository Management
- Check status: `./mib status`
- Update all repos: `./mib update-repos`
- Format the sources of every repository that sets `Repository.Format`: `./mib format`, or `./mib format --check` to report only
- Validate the tracked text files of those repositories: `./mib validate`, `./mib validate --staged`, or `./mib validate --base REF`
- Run the clang-tidy checks the `.clang-tidy` files enable over every repository that sets `Repository.Tidy {Enabled: true}` (`IncludeFiles`/`ExcludeFiles` wildcards, relative to the repository, narrow its files): `./mib tidy`, or `./mib tidy --staged` / `--base REF` for changed files; generate the build system first
- `mib` supports git worktrees, but avoid them for agent-driven workflows in this repository. Many changes land inside the `Malterlib/*` sub-repositories, and syncing those changes back from the top-level worktree does not behave correctly.
- Keep the main repository on a named branch when bootstrapping or running `./mib update-repos`. Detached `HEAD` checkouts are intentionally unsupported because sub-repositories are synchronized against the main branch.
- The system uses git LFS for binary dependencies - ensure it's installed
- Many directories under `Malterlib/` are separate Git repositories. When checking status or searching history, run Git commands inside the relevant subdirectory (`Malterlib/Concurrency`, `Malterlib/Cloud`, etc.) or use the helper scripts (`./mib git ...`) that fan out across sub-repos.

### Sub-Repositories
A checkout is a tree of Git repositories that `mib` manages: the root, which is Malterlib itself or a product built on it,
and the sub-repositories in `Malterlib/*`, `External/*` and `Binaries/*`, plus whatever a product adds. Run Git in the
repository that owns the files (`git -C Malterlib/Concurrency log`), or `./mib git ...` to run it in all of them.

A parent pins each sub-repository to a commit in a committed `.MRepo` file. The root's `Malterlib.MRepo` pins
`Malterlib/Core`, which pins the other `Malterlib/*` modules in `Malterlib/Core/Malterlib.MRepo`, and a module pins the
external repositories it uses in its own `External.MRepo`, as `Malterlib/Tool/External.MRepo` pins `External/ninja`.
Beside each is a gitignored `.MRepoState` with the commit mib last saw checked out, which is how `./mib update-repos`
tells which side moved:

- A sub-repository's HEAD moved (you committed in it): mib writes the new HEAD into the parent's `.MRepo`, which then
  shows as modified in the parent. `./mib status` does this too, and lists what needs committing or pushing in every
  repository.
- The pin moved (a checkout, pull or rebase in the parent): the sub-repository has to follow. mib does that by itself
  only when it creates the sub-repository's branch or the branch is already at the pin. Otherwise it stops with a
  recommended action per sub-repository (reset, rebase, leave or manual) and the `--reconcile` commands that apply them.

### Reconciling Sub-Repositories
Reconcile each repository mib lists on its own, never with a `*` wildcard, and research each one to choose its action;
mib's recommendation is a starting point, not the answer. `update-repos` has no pretend mode, so these checks are the only
preview. mib prints each repository as `<action> recommended for <from> -> <pin>`, prefixed with
`Branch switch to '<branch>':` when the branch changes too.

- Reset moves the branch to the pin and drops what is not in it; without a branch switch it is a `git reset --hard`.
- Rebase replays the branch's own commits on the pin.
- Leave keeps the repository as it is, and its HEAD becomes the parent's new pin.

1. `git -C <repo> fetch`, so the remote-tracking branches are current.
2. `git -C <repo> log --oneline <pin>..<from>` and `git -C <repo> log --oneline <from>..<pin>` show what each side has
   that the other lacks, and `git -C <repo> log --oneline <from> --not --remotes <pin>` the commits that exist only
   locally: on no remote and not in the pin. Reset drops those.
3. `git -C <repo> status --porcelain` shows uncommitted changes, which a reset without a branch switch drops. With a
   branch switch, mib stashes them under the branch it leaves.
4. Decide from what the commits are: reset when the local side has nothing that isn't on a remote or in the pin, rebase
   when its own commits belong on top of the pin, leave when its HEAD is what the parent should pin. Apply the actions
   per repository, naming them as mib does: `./mib update-repos '--reconcile=Malterlib/Core:reset,External/ninja:rebase'`.

When it is unclear which action keeps the right work, report what each side has with mib's recommendation and let the
user choose.

### What Happens on Checkout
mib installs a `post-checkout` hook in the root that runs `./mib update-repos` after `git checkout` or `git switch`
there, so the sub-repositories follow the root's branch:

- With the root on its default branch, each sub-repository goes to its own default branch.
- With the root on any other branch, every sub-repository goes to a branch of the same name. An existing branch is
  checked out; a missing one is created at the commit pinned in the `.MRepo`.
- Uncommitted changes in a sub-repository that switches branch are stashed as `mib-branch-switch:<repo>:<branch>:<id>`,
  named for the branch it leaves, and popped when that repository switches back to that branch; they don't follow to
  the new one. Stashes for branches nobody returns to stay behind. Leave
  them, and ask the user before dropping one.

All of this is expected. Mention a stash in your summary, but don't treat the switch as a problem to review or undo, and
don't avoid the hook by building commits with Git plumbing or by disabling hooks: the hook keeps the sub-repositories in
step at every branch switch, where syncing by hand leaves them to be reconciled later without a view of what is in them.
To read another branch without switching every repository, use `git show <branch>:<path>`, `git log <branch>` or
`git diff <branch>` rather than checking it out.

The hook does nothing during a rebase, merge or cherry-pick, on a detached HEAD, or when mib itself is doing the
checkout. `git pull` and `git merge` have no such hook, so after pulling, merging or rebasing the root, run
`./mib update-repos`.

### Creating and Switching Branches
- `./mib branch <name>` switches the root to `<name>`, creating it from the current commit if it doesn't exist, then
  updates the sub-repositories as above. Use it to switch between branches, and to start a new branch from a clean
  tree, typically the default branch. Uncommitted changes, in the root as well, are stashed under the branch you leave
  and restored when you `./mib branch` back to it. New sub-repository branches start at the pins in the root's last
  commit, so sub-repository commits not yet pinned in a root commit stay on the old branch.
- `./mib unbranch` does the same back to the default branches: `master` in the root, and each sub-repository's own
  default branch, such as `master`, or `malterlib` in the forks of third-party repositories.
- `./mib branch -f <name>` runs `git checkout -B <name>` in the root and every sub-repository, keeping each one's current
  commit and uncommitted changes, and runs no update. Use it to copy the current branch, with everything in progress,
  to a new name: a feature branch when you already started changing files on the default branch or another feature's
  branch, a new branch that splits work off the current one, or a backup before rewriting history.
- `./mib unbranch -f` is the same copy onto each repository's own default branch, for work in progress on a feature
  branch that belongs on the default branches instead.

`-f` moves a branch that already exists to the current commit, even backwards, in every repository, so preview it with
`-p` (pretend) first. `-n <wildcard>` limits it to matching repositories.

### Committing and Pushing Sub-Repository Changes
Commit in the sub-repository first, then record the new pins: `./mib repo-commit` commits every changed `.MRepo` up the
chain (a `Malterlib/String` commit changes `Malterlib/Core/Malterlib.MRepo`, and that commit changes the root's
`Malterlib.MRepo`) with `Update repositories` messages that list the sub-repository commits, and refreshes those commits
after sub-repository commits are rewritten. Use it instead of editing or committing `.MRepo` files by hand.
`./mib repo-commit --skip-ci` adds `[skip ci]` to the messages, for pushes that should not start CI.

`./mib push` pushes only the repositories whose branch differs from origin's default branch, deepest first, so a pushed
pin never names a commit others can't fetch. `./mib push -p` (pretend) lists the `git push` commands it would run, group
by group, without pushing, including which repositories get a new branch on origin; check it before pushing.

### Working with Workspaces
Workspaces are collections of build targets. Common workspaces include:
- **Tests** - All test targets
- **MTool** - Build system tools
- **Malterlib_[Module]** - Specific module workspaces
- **Apps_Malterlib_[Module]** - Application workspaces

Generate a workspace before building: `./mib generate [WorkspaceName]`

### Several Generated Build Systems
`./mib generate`, `./mib build` and `./mib build-target` take `--output-directory <Dir>`, the generated build system
to use, which is `BuildSystem/Default` unless it is given. Each build system has its own `UserSettings.MSettings`,
`SharedBuildSettings.sh`, compiled files and configurations. A build system that has not been generated yet is
generated for Ninja by the first build in it.

A build that finds another build of the same configuration running is refused, so that agents notice. `--wait-for-lock`
(or `MalterlibBuildLockWait=true`) waits for it instead, and `--lock-timeout <Seconds>` (or `MalterlibBuildLockTimeout`)
waits at most that long. Ninja reports the build it waits for or refuses from the `.ninja_build_lock.owner` file beside its
lock.

- Builds that another build runs, such as of host tools, always wait. A build that would run a build in its own build
  directory, or nest builds more than 8 deep, stops instead (`NINJA_BUILD_LOCK_CHAIN`).
- Generations of one build system lock `.mib_generate_lock` in its output directory. `./mib generate` refuses while
  another generation runs, unless it is given `--wait-for-lock` or `--lock-timeout`; the generation that a build runs
  always waits.
- Commands that update repositories wait for each other through `BuildSystem/.mib_repositories_lock`.
- Ninja and the build scripts only wait with an MTool of `MToolVersion` 6 or later (`MalterlibBuildLocking`): the
  bootstrap MTool that generates the build system decides, so that older bootstrap binaries still build.

### Host Tools
Tools that builds run on the build host, such as the shader compiler of Render, are built by their own build system,
`BuildSystem/HostTools`, for the host platform and architecture in Release without link time code generation, with
reproducible builds and the same branch and version in their defines and version information for every branch and
version (`MalterlibFixedVersionDefines`), so that they are not rebuilt when those change. It deploys them to
`BuildSystem/HostTools/Binaries`, where every build runs them from whatever configuration its `UserSettings.MSettings`
builds, so a tool can be built in Debug to debug it.

- A module adds the target of a tool to `MalterlibHostTools`.
- Tools are built in groups, such as the tools that use the same imports. A module defines a `Tool` target for a group
  with `Property.HostToolsTargets` set to the targets of its tools and the files they deploy as `CommandLine_Outputs`.
  In every build it generates `BuildSystem/HostTools` and runs
  `./mib build-target --output-directory BuildSystem/HostTools HostTools <Targets>`, which is a no-op when nothing has
  changed, and ninja reruns what uses a tool only when the tool changed.
- The build that runs the host tools generates them with an environment of their own: ninja replaces the environment
  of the build for that command (`CommandLine_Environment`) with the variables of
  `MalterlibHostToolsCopiedEnvironmentVariables`, copied from its own environment, so that settings meant for the build
  that runs them stay out, and `BuildSystem/HostTools` does not generate itself. Ninjas before MToolVersion 6, and the
  Visual Studio and Xcode generators, cannot replace the environment of a command, so the command unsets the other
  exported variables itself.
- Targets that run a tool depend on the target of its group and have the file of the tool among their inputs.
- The groups build one at a time, in the ninja pool `HostToolsPool`, since they share one build directory.
- Cross compiled builds run the tools of the host, so the targets of host tools only exist in that build system
  (`MalterlibHostToolsBuildSystem`), as do the imports that only they use.
- The build system of host tools only generates the `HostTools` workspace. `MalterlibGenerateWorkspaces` in the
  `UserSettings.MSettings` of a build system sets which workspaces it generates.
- The tests of the Tool module build and run a test host tool, `TestHostTool`, so every `Tests` build with the Tool
  module builds the host tools from inside ninja.

### Import Caches
What CMake configures for an `%Import` is cached in the `ImportCache` directory of the module and committed, so that
builds do not run CMake. `MalterlibGenerateImportCaches true` in the `UserSettings.MSettings` of a build system makes
`./mib generate --output-directory <Dir>` generate them for every platform, architecture and configuration that the
host can: macOS and Linux on macOS, Windows on Windows. Do it in each build system with imports, including
`BuildSystem/HostTools`, commit the caches and remove the setting. Builds are refused while it is set.

## Code Standards

### Declarations and Implementations

- `.h` files contain declarations and type definitions only. Never put function implementations
  directly in `.h` files, including member functions, templates, and inline or `constexpr` functions.
  This keeps the interface overview compact and discoverable.
- `.hpp` files contain implementations that need to be included, such as template and inline function
  implementations. Keep their declarations in `.h` files.
- `.cpp` files contain all other implementations.

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
- Use `struct`, not `class`, for Malterlib types. Do not add a leading `public:` section just to expose the default interface. Only use `class` when editing third-party code or when an external API requires that exact spelling.

#### Parameter Ownership
- For non-coroutine function parameters that would otherwise take an owning object by value, choose explicit semantics: use `const &` when borrowing/read-only, or `&&` when consuming/moving from the argument.
- Do not use by-value parameters as a default sink pattern just to allow temporaries and lvalues. If a callsite needs to pass an lvalue to a consuming `&&` parameter, make the copy explicit at the callsite with `fg_TempCopy`.
- Coroutine parameters are the exception: follow coroutine safety requirements and pass values as needed so data survives suspension.

#### Unused Parameters
- Never silence an unused parameter with a `(void)_Param;` cast. Keep the parameter named and leave it unused: the name tells what the parameter is. The builds do not warn on unused parameters, only on unused variables.

#### Logical Unit Spacing
- Separate distinct logical units with one blank line: setup, validation, operation execution, result capture, assertions, cleanup, and terminal `return`/`co_return`.
- Do not add a blank line immediately after an opening `{` before the first statement.
- Do not add a blank line immediately after `case` or `default` labels before the case body.
- In switch cases, keep one-statement return cases compact. For multi-statement cases, separate the case work from the case-level `break;` with a blank line.
- Keep fold-expression statements ending in `, ...` closed as `);` on the same line. This is the exception to the split-statement semicolon rule.
- For local `using namespace` declarations, add a blank line before subsequent code.
- For lambdas assigned to variables, indent the lambda body braces one continuation level under the lambda expression, and put the terminating `;` on its own statement-indentation line.

#### Statement Splitting
- Each substatement on the same logical level goes on its own line
- When a condition or expression is split across lines, put each logical unit on its own line
- Matching `(` and `)` in a split construct stay at the same indentation level
- Scope markers must be on separate lines
- Complex statements should be broken down for readability
- Continuation indentation depends on whether the current statement or expression may be extended afterward
- If it may be extended afterward, indent the continued part one extra level
- If it may not be extended afterward, do not add the extra continuation indent
- Exception: if the statement itself starts with `(`, keep that `(` at the statement indentation level instead of adding a continuation indent
- When a split function call is already a logical unit inside another split expression, align the call's `(` and matching `)` with the function expression; do not add another continuation level solely for the argument list
- If a function declaration or definition is split to fit the line length, first convert it to trailing return type
- If it still does not fit, put the trailing return type on its own line; do not put the return type on a separate line before the function name
- Do not split function declarations or definitions by putting some parameters on the first line and remaining parameters on continuation lines. Either keep the full parameter list on one line or split the whole parameter list under an indented `(`.
- When control-flow conditions such as `if` are split across lines, use braces even for a single statement to keep the structure clear
- When a split statement, expression, or declaration ends with `;`, put the `;` on its own line at the statement indentation level

```cpp
if
(
	Condition
)
{
	Statement();
}
```

```cpp
if
(
	!pValue->f_Check
	(
		Argument
	)
)
{
	Statement();
}
```

```cpp
auto fg_Function
	(
		int _Param
	)
	-> void
;
```

```cpp
auto Value = fg_Function
	(
		5
	)
	+ 6
;
```

```cpp
fg_Function
	(
		5
	)
;
```

```cpp
(
	[&]
	{
		f_DoWork();
		return true;
	}
	()
	, ...
);
```

### Comment Style and Verbosity

Code should communicate its behavior primarily through names, types, structure, and tests. Comments
are for important information the code cannot express clearly.

#### Make the Code Explain Itself

- Represent meaningful states with enums or separate types instead of undocumented integers or
  clusters of interacting booleans.
- Put validation and invariants close to the data or operation they protect. Make invalid states
  difficult to construct where practical.
- Remove obsolete parameters, branches, and abstractions. Do not explain why dead structure remains.

#### Write Comments for Durable Information

A useful comment records something a maintainer needs for correctness but cannot readily infer:

- why an obvious or simpler approach is wrong;
- ownership, lifetime, nullability, sentinel, error, or threading contracts;
- non-local invariants and ordering requirements;
- security, portability, protocol, or platform constraints;
- the intent behind an inherently subtle algorithm.

State the constraint directly and as briefly as possible. Prefer present-tense explanations of why
the code must have its current shape. Use `// ` for ordinary Malterlib comments, aligned with the code,
and continue multiline explanations with `//`.

For example, this excerpt from
`Malterlib/Concurrency/Source/Actor/Malterlib_Concurrency_AsyncGeneratorPrivate.hpp` explains how
the coroutine can become invalid. The condition alone does not convey those causes.

```cpp
if (!KeepAlive.f_HasValidCoroutine())
	co_return {}; // Can happen when f_Suspend throws or co-routines are aborted in fp_DestroyInternal
```

#### Do Not Use Comments To

- narrate the next statement or restate names and control flow;
- describe previous implementations, fixes, patches, or reviewer discussions;
- list every current caller, backend, test, example, or implementation step;
- duplicate user documentation or architecture documentation;
- speculate about future features;
- compensate for unclear naming or unnecessarily complicated structure.

History belongs in version control. User-facing behavior belongs in the relevant module's documentation.
Broad architecture belongs in the appropriate `CLAUDE.md` or a focused design document; regenerate
`AGENTS.md` with `./mib update-agents` when changing its source instructions. Tests should express
regressions through a specific test name, fixture, and assertions; add a comment only when the scenario
remains unclear.

#### Keep Contracts Local

- Put function documentation immediately before the function implementation, not at its declaration
  in the header. This keeps the interface overview compact and discoverable. Document inputs, outputs,
  ownership, lifetime, errors, and constraints callers must honor there, in the `.hpp` or `.cpp` file.
- Comments inside implementations explain local rationale and implementation invariants.
- Field comments clarify semantics that the field's name and type cannot carry. Keep each field comment
  to one line after the field, within the 190-column limit. Only when indentation leaves insufficient
  room may the comment occupy one line immediately before the field; do not use multiline field comments.

When changing code, verify nearby comments still add information and remain true. Shorten, move, or
delete them when they do not. A long comment is a prompt to consider better structure or a focused
document, not an automatic requirement to split the code.

### Naming Conventions

#### General Rules
- Use Upper Camel Case for most names
- Exceptions: Language functionality emulation uses lower case (e.g., `inline_always`, `uint32`)

#### Function Prefixes
- `f_`: Member function
- `fp_`: Private/protected member function
- `fs_`: Static member function
- `fsp_`: Private/protected static member function
- `fg_`: Global function (also used in anonymous namespaces)
- `fsg_`: Static global function (only when explicitly declared `static`, not for anonymous namespaces)

#### Parameter Prefixes
- `_`: Standard function parameter
- `p_`: Function parameter pack
- `o_`: Output parameter
- `po_`: Output parameter pack
- `t_`: Template parameter
- `tp_`: Template parameter pack
- `d_`: Macro parameter
- Name every parameter, also an unused one and the parameters of function types, like `TCFunction<void (CStr _Message)>`. Only the `int` that tells a postfix `operator ++` or `--` apart stays unnamed.

#### Variable Prefixes
- No prefix for local variables
- `c_`: Compile-time constant local variables
- `s_`: Static local variables
- `g_`: Global variables (also used in anonymous namespaces)
- `gc_`: Compile-time constant global variables (also used in anonymous namespaces)
- `gs_`: Static global variables (only when explicitly declared `static`, not for anonymous namespaces)
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

- `umint` (or memory integer) is an unsigned integer type sized for memory/count/index use on the target platform. Its signed counterpart is `smint`, while `aint` is a separate signed arithmetic/result type and should not be treated as the signed form of `umint`.
- The build system uses absolute paths by default
- Build artifacts are placed in `/opt/Deploy/`, `/Deploy/` or `/c/Deploy/` depending on the OS and `BuildSystem/Default/PostCopy.MConfig`
- The system supports cross-compilation for multiple platforms
- Use `./mib --help` for detailed command information
- The project uses custom memory management with configurable allocators
- LFS (Large File Storage) is used for binary dependencies
- The build system caches environment and dependency information in `BuildSystem/Default/`
- When switching branches, run `./mib update-repos` to ensure all repositories are synchronized
- The mib script automatically bootstraps required tools on first use
- **Bit utilities**: `fg_GetHighestBitSet(x)` safely handles `x == 0` (returns a defined value). `fg_GetHighestBitSetNoZero(x)` is the faster variant that requires `x != 0` — passing 0 is undefined behavior. Use `fg_GetHighestBitSet` when zero is a possible input; use `fg_GetHighestBitSetNoZero` only when zero has been excluded by a prior check.

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
#ifdef DPlatformFamily_macOS
	// macOS-specific code
#elif DPlatformFamily_Windows
	// Windows-specific code
#elif DPlatformFamily_Linux
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
	mc_None = 0,
	, mc_Flag1 = DMibBit(0)
	, mc_Flag2 = DMibBit(1)
	, mc_Flag3 = DMibBit(2)
};

// Use enum operators
void fg_UseFlags()
{
	EMyFlags Flags = EMyFlags::mc_Flag1 | EMyFlags::mc_Flag2;

	if (fg_IsSet(Flags, EMyFlags::mc_Flag1))
	{
		// Flag1 is set
		fg_ProcessFlag1();
	}

	nFlags &= ~EMyFlags::mc_Flag2;  // Clear Flag2
}
```

### Thread Priority

`EExecutionPriority` is absolute, and on Linux a thread cannot raise itself above what it inherits
from its creator. Read `Documentation/ThreadPriority.md` before changing thread priorities, thread
creation or the priority of Linux daemons.

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

**Legacy Implementation Headers (.imp.h)**
These existing filenames predate the declaration/implementation separation above. Use `.hpp` for
implementations that need to be included; do not introduce new function implementations in `.imp.h` files.

- `Malterlib_Core_PlatformImp_POSIX.imp.h` - POSIX implementation templates
- `Malterlib_Core_PlatformImp_*_Net.imp.h` - Network implementations
- `Malterlib_Core_PlatformImp.imp.h` - Core platform implementation

**Included Implementations (.hpp)**
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

#ifdef DPlatformFamily_Windows
	Path = NPlatform::fg_GetEnvironmentVariable("APPDATA");
	Path += "\\MyApp";
#elif DPlatformFamily_macOS
	Path = NPlatform::fg_GetHomeDirectory();
	Path += "/Library/Application Support/MyApp";
#elif DPlatformFamily_Linux
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
4. **Check platform at compile-time** - Use DPlatformFamily_* macros
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
- **Develop**: `../Develop/CLAUDE.md` - Reusable editor, IDE, and developer-tool support, including EditorConfig
- **File**: `../File/CLAUDE.md` - File system operations
- **Function**: `../Function/CLAUDE.md` - Function utilities and delegates
- **Intrusive**: `../Intrusive/CLAUDE.md` - Intrusive data structures
- **Network**: `../Network/CLAUDE.md` - Networking and communication
- **Numeric**: `../Numeric/CLAUDE.md` - Numerical operations and math
- **Process**: `../Process/CLAUDE.md` - Process management and IPC
- **Stream**: `../Stream/CLAUDE.md` - Stream processing and I/O
- **Test**: `../Test/CLAUDE.md` - Test framework usage and test-writing conventions
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
