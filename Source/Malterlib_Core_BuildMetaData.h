// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	struct CBuildMetadata
	{
		char const *m_pProduct;
		char const *m_pApplication;
		char const *m_pConfiguration;
		char const *m_pGitBranch;
		char const *m_pGitCommit;
		char const *m_pPlatform;
		char const *m_pVersion;
		unsigned m_nTags = 0;
		char const * const *m_pTags;
	};
}
