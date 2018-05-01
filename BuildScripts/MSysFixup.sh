#!/bin/bash
# Copyright © 2015 Hansoft AB 
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# MSys imports all env vars by uppercasing their names.
# The build system is case sensitive here so we need to convert them
# back.
# TODO: A proper fix.

# fixupVar <RealName> <AltName>
fixupVar() {
	if [[ -z ${!1} ]] ; then
		eval "export $1=${!2}"
	fi
}

#if [[ "$MalterlibPlatform" == "Windows" ]] ; then
	fixupVar MalterlibAutoBuild MALTERLIBAUTOBUILD
	fixupVar MalterlibAutoBuildMTool MALTERLIBAUTOBUILDMTOOL
	fixupVar MalterlibAutoBuildTestPutPath MALTERLIBAUTOBUILDTESTPUTPATH
	fixupVar MalterlibAutoBuildTestRoot MALTERLIBAUTOBUILDTESTROOT
	fixupVar MalterlibBranch MALTERLIBBRANCH
	fixupVar MalterlibFullBranch MALTERLIBFULLBRANCH
	fixupVar MalterlibBranchOnlyLast MALTERLIBBRANCHONLYLAST
	fixupVar MalterlibFullBranchOnlyLast MALTERLIBFULLBRANCHONLYLAST
	fixupVar MalterlibBuildServerTest MALTERLIBBUILDSERVERTEST
	fixupVar MalterlibCompiledFiles MALTERLIBCOMPILEDFILES
	fixupVar MalterlibDeployRoot MALTERLIBDEPLOYROOT
	fixupVar MalterlibDeploySymbols MALTERLIBDEPLOYSYMBOLS
	fixupVar MalterlibPreBuildNoClean MALTERLIBPREBUILDNOCLEAN
	fixupVar MalterlibSigning MALTERLIBSIGNING
	fixupVar DisablePostCopy DISABLEPOSTCOPY
	fixupVar DisableProjectGen DISABLEPROJECTGEN
	fixupVar HansoftBuildingAutoBuild HANSOFTBUILDINGAUTOBUILD
	fixupVar MalterlibDeploySymbolsPath MALTERLIBDEPLOYSYMBOLSPATH
	fixupVar MalterlibRepositoryHardReset MALTERLIBREPOSITORYHARDRESET
#fi


