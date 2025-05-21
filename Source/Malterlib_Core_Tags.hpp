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
			, TCEnableIf<NPrivate::TCAreTagsCompatible<TCTags<tfp_CTags...>, TCTags>::mc_Value> * = nullptr
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
			using CType = t_CDefaultTag;
		};

		template <typename t_CBaseTag, typename t_CDefaultTag, typename t_CTag0, typename ...tp_CTags>
		struct TCEvalTag<t_CBaseTag, t_CDefaultTag, t_CTag0, tp_CTags...>
		{
			using CType = TCConditional
				<
					NMib::NTraits::cIsConvertible<t_CTag0, t_CBaseTag>
					, t_CTag0
					, typename TCEvalTag<t_CBaseTag, t_CDefaultTag, tp_CTags...>::CType
				>
			;
		};
		
		
		template <typename t_COtherTags, typename ...tp_CTags>
		struct TCEvalTagCompatible;

		template <typename ...tp_COtherTags, typename t_CTag, typename ...tp_CTags>
		struct TCEvalTagCompatible<TCTags<tp_COtherTags...>, t_CTag, tp_CTags...>
		{
			using CBaseTag = NMib::NTraits::TCGetBase<t_CTag>;
			using COtherTag = typename NPrivate::TCEvalTag<CBaseTag, CBaseTag, tp_COtherTags...>::CType;

			enum
			{
				mc_Value = NMib::NTraits::cIsConvertible<COtherTag, t_CTag>
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
			using CType = t_CResultingTags;
		};
		
		template <typename t_CTags, typename t_CBaseTag>
		struct TCEvalHasTagType;

		template <typename t_CTag0, typename ...tp_CTags, typename t_CBaseTag>
		struct TCEvalHasTagType<TCTags<t_CTag0, tp_CTags...>, t_CBaseTag>
		{
			enum
			{
				mc_Value = NMib::NTraits::cIsSame<t_CBaseTag, t_CTag0> || TCEvalHasTagType<TCTags<tp_CTags...>, t_CBaseTag>::mc_Value
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
			using CType = TCConditional
				<
					!TCEvalHasTagType<TCTags<tp_CTagsToRemove...>, NMib::NTraits::TCGetBase<t_CTag0>>::mc_Value
					//NMib::NTraits::cIsConvertible<t_CTag0, t_CBaseTag>
					, typename TCEvalRemoveTags<TCTags<tp_CTagsToRemove...>, TCTags<tp_CResultingTags..., t_CTag0>, TCTags<tp_CTags...>>::CType
					, typename TCEvalRemoveTags<TCTags<tp_CTagsToRemove...>, TCTags<tp_CResultingTags...>, TCTags<tp_CTags...>>::CType
				>
			;
		};
		
		
	}
	
	template <typename ...tp_CTags, typename t_CBaseTag, typename t_CDefaultTag>
	struct TCGetTag<TCTags<tp_CTags...>, t_CBaseTag, t_CDefaultTag>
	{
		using CType = typename NPrivate::TCEvalTag<t_CBaseTag, t_CDefaultTag, tp_CTags...>::CType;
	};
	
	template <typename ...tp_CTags, typename t_CBaseTag>
	struct TCHasTag<TCTags<tp_CTags...>, t_CBaseTag>
	{
		using CBaseTag = NMib::NTraits::TCGetBase<t_CBaseTag>;
		enum
		{
			mc_Value = NTraits::cIsConvertible<typename TCGetTag<TCTags<tp_CTags...>, CBaseTag>::CType, t_CBaseTag>
		};
	};		
	
	template <typename ...tp_CTags, typename ...tp_CTagsToRemove>
	struct TCRemoveTags<TCTags<tp_CTags...>, tp_CTagsToRemove...>
	{
		using CType = typename NPrivate::TCEvalRemoveTags<TCTags<tp_CTagsToRemove...>, TCTags<>, TCTags<tp_CTags...>>::CType;
	};

	template <typename ...tp_CTags, typename ...tp_CTagsToAdd>
	struct TCAddTags<TCTags<tp_CTags...>, tp_CTagsToAdd...>
	{
		using CType = TCTags<tp_CTags..., tp_CTagsToAdd...>;
	};
}
