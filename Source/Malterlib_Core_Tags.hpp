// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once
#include <Mib/Type/Traits>

namespace NMib
{
	namespace NPrivate
	{
		template <typename t_CTags0, typename t_CTags1>
		struct TCAreTagsCompatible;
	}		

	template <typename ...tfp_CTag>
	struct TCTags
	{
		template 
		<
			typename ...tfp_CTags
			, typename TCEnableIf<NPrivate::TCAreTagsCompatible<TCTags<tfp_CTags...>, TCTags>::mc_Value>::CType * = nullptr
		>
		TCTags(TCTags<tfp_CTags...>)
		{
		}
		TCTags()
		{
		}
	};
	
	namespace NPrivate
	{

		template <typename t_CBaseTag, typename t_CDefaultTag, typename ...tp_CTags>
		struct TCEvalTag;

		template <typename t_CBaseTag, typename t_CDefaultTag>
		struct TCEvalTag<t_CBaseTag, t_CDefaultTag>
		{
			typedef t_CDefaultTag CType;
		};

		template <typename t_CBaseTag, typename t_CDefaultTag, typename t_CTag0, typename ...tp_CTags>
		struct TCEvalTag<t_CBaseTag, t_CDefaultTag, t_CTag0, tp_CTags...>
		{
			typedef typename TCChooseType
				<
					NMib::NTraits::TCIsConvertible<t_CTag0, t_CBaseTag>::mc_Value
					, t_CTag0
					, typename TCEvalTag<t_CBaseTag, t_CDefaultTag, tp_CTags...>::CType
				>::CType CType
			;
		};
		
		
		template <typename t_COtherTags, typename ...tp_CTags>
		struct TCEvalTagCompatible;

		template <typename ...tp_COtherTags, typename t_CTag, typename ...tp_CTags>
		struct TCEvalTagCompatible<TCTags<tp_COtherTags...>, t_CTag, tp_CTags...>
		{
			typedef typename NMib::NTraits::TCGetBase<t_CTag>::CType CBaseTag;
			typedef typename NPrivate::TCEvalTag<CBaseTag, CBaseTag, tp_COtherTags...>::CType COtherTag;
			
			enum
			{
				mc_Value = NMib::NTraits::TCIsConvertible<COtherTag, t_CTag>::mc_Value
					&& TCEvalTagCompatible<TCTags<tp_COtherTags...>, tp_CTags...>::mc_Value
			};
		};

		template <typename ...tp_COtherTags>
		struct TCEvalTagCompatible<TCTags<tp_COtherTags...>>
		{
			enum
			{
				mc_Value = true
			};
		};
		
		template <typename ...tp_CTags0, typename ...tp_CTags1>
		struct TCAreTagsCompatible<TCTags<tp_CTags0...>, TCTags<tp_CTags1...>>
		{
			enum
			{
				mc_Value = NPrivate::TCEvalTagCompatible<TCTags<tp_CTags0...>, tp_CTags1...>::mc_Value
			};
		};
		
		template <typename t_CTagsToRemove, typename t_CResultingTags, typename t_CTags>
		struct TCEvalRemoveTags;

		template <typename t_CTagsToRemove, typename t_CResultingTags>
		struct TCEvalRemoveTags<t_CTagsToRemove, t_CResultingTags, TCTags<>>
		{
			typedef t_CResultingTags CType;
		};
		
		template <typename t_CTags, typename t_CBaseTag>
		struct TCEvalHasTagType;

		template <typename t_CTag0, typename ...tp_CTags, typename t_CBaseTag>
		struct TCEvalHasTagType<TCTags<t_CTag0, tp_CTags...>, t_CBaseTag>
		{
			enum
			{
				mc_Value = NMib::NTraits::TCIsSame<t_CBaseTag, t_CTag0>::mc_Value || TCEvalHasTagType<TCTags<tp_CTags...>, t_CBaseTag>::mc_Value
			};
		};

		template <typename t_CBaseTag>
		struct TCEvalHasTagType<TCTags<>, t_CBaseTag>
		{
			enum
			{
				mc_Value = false
			};
		};

		template <typename ...tp_CTagsToRemove, typename ...tp_CResultingTags, typename t_CTag0, typename ...tp_CTags>
		struct TCEvalRemoveTags<TCTags<tp_CTagsToRemove...>, TCTags<tp_CResultingTags...>, TCTags<t_CTag0, tp_CTags...>>
		{
			typedef typename TCChooseType
				<
					!TCEvalHasTagType<TCTags<tp_CTagsToRemove...>, typename NMib::NTraits::TCGetBase<t_CTag0>::CType>::mc_Value
					//NMib::NTraits::TCIsConvertible<t_CTag0, t_CBaseTag>::mc_Value
					, typename TCEvalRemoveTags<TCTags<tp_CTagsToRemove...>, TCTags<tp_CResultingTags..., t_CTag0>, TCTags<tp_CTags...>>::CType
					, typename TCEvalRemoveTags<TCTags<tp_CTagsToRemove...>, TCTags<tp_CResultingTags...>, TCTags<tp_CTags...>>::CType
				>::CType
				CType
			;
		};
		
		
	}
	
	template <typename ...tp_CTags, typename t_CBaseTag, typename t_CDefaultTag>
	struct TCGetTag<TCTags<tp_CTags...>, t_CBaseTag, t_CDefaultTag>
	{
		typedef typename NPrivate::TCEvalTag<t_CBaseTag, t_CDefaultTag, tp_CTags...>::CType CType;
	};
	
	template <typename ...tp_CTags, typename t_CBaseTag>
	struct TCHasTag<TCTags<tp_CTags...>, t_CBaseTag>
	{
		typedef typename NMib::NTraits::TCGetBase<t_CBaseTag>::CType CBaseTag;
		enum
		{
			mc_Value = NTraits::TCIsConvertible<typename TCGetTag<TCTags<tp_CTags...>, CBaseTag>::CType, t_CBaseTag>::mc_Value
		};
	};		
	
	template <typename ...tp_CTags, typename ...tp_CTagsToRemove>
	struct TCRemoveTags<TCTags<tp_CTags...>, tp_CTagsToRemove...>
	{
		typedef typename NPrivate::TCEvalRemoveTags<TCTags<tp_CTagsToRemove...>, TCTags<>, TCTags<tp_CTags...>>::CType CType;
	};		

	template <typename ...tp_CTags, typename ...tp_CTagsToAdd>
	struct TCAddTags<TCTags<tp_CTags...>, tp_CTagsToAdd...>
	{
		typedef TCTags<tp_CTags..., tp_CTagsToAdd...> CType;
	};		
	
}