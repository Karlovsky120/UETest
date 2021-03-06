// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/NavigationModifier.h"
#include "AI/NavigationOctree.h"
#include "RecastHelpers.h"
#include "ConvexHull2d.h"
#if WITH_RECAST
#include "PImplRecastNavMesh.h"
#endif // WITH_RECAST

// if square distance between two points is less than this the those points
// will be considered identical when calculating convex hull
// should be less than voxel size (recast navmesh)
static const float CONVEX_HULL_POINTS_MIN_DISTANCE_SQ = 4.0f * 4.0f;

//----------------------------------------------------------------------//
// FNavigationLinkBase
//----------------------------------------------------------------------//
FNavigationLinkBase::FNavigationLinkBase()
	: MaxFallDownLength(1000.0f), Direction(ENavLinkDirection::BothWays), UserId(0), SnapRadius(30.f)
{
	AreaClass = NULL;
	SupportedAgentsBits = 0xFFFFFFFF;
}

//----------------------------------------------------------------------//
// UNavLinkDefinition
//----------------------------------------------------------------------//
UNavLinkDefinition::UNavLinkDefinition(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	, bHasDeterminedMetaAreaClass(false)
	, bHasMetaAreaClass(false)
	, bHasDeterminedAdjustableLinks(false)
	, bHasAdjustableLinks(false)
{
}

const TArray<FNavigationLink>& UNavLinkDefinition::GetLinksDefinition(UClass* LinkDefinitionClass)
{
	static const TArray<FNavigationLink> DummyDefinition;

	return (LinkDefinitionClass != NULL/* && LinkDefinitionClass->IsA(StaticClass())*/) 
		? ((UNavLinkDefinition*)LinkDefinitionClass->GetDefaultObject())->Links
		: DummyDefinition;
}

const TArray<FNavigationSegmentLink>& UNavLinkDefinition::GetSegmentLinksDefinition(UClass* LinkDefinitionClass)
{
	static const TArray<FNavigationSegmentLink> DummyDefinition;

	return (LinkDefinitionClass != NULL/* && LinkDefinitionClass->IsA(StaticClass())*/) 
		? ((UNavLinkDefinition*)LinkDefinitionClass->GetDefaultObject())->SegmentLinks
		: DummyDefinition;
}

#if WITH_EDITOR
void UNavLinkDefinition::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	// In case relevant data has changed, clear the flag that says we've determined whether there's a meta area class
	// so it will be recalculated the next time it's needed.
	bHasDeterminedMetaAreaClass = false;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool UNavLinkDefinition::HasMetaAreaClass() const
{
	if (bHasDeterminedMetaAreaClass)
	{
		return bHasMetaAreaClass;
	}

	bHasDeterminedMetaAreaClass = true;
	for (TArray<FNavigationLink>::TConstIterator LinkIt(Links); LinkIt; ++LinkIt)
	{
		if ((LinkIt->AreaClass != NULL) && LinkIt->AreaClass->IsChildOf(UNavAreaMeta::StaticClass()))
		{
			bHasMetaAreaClass = true;
			return true;
		}
	}
	for (TArray<FNavigationSegmentLink>::TConstIterator LinkIt(SegmentLinks); LinkIt; ++LinkIt)
	{
		if ((LinkIt->AreaClass != NULL) && LinkIt->AreaClass->IsChildOf(UNavAreaMeta::StaticClass()))
		{
			bHasMetaAreaClass = true;
			return true;
		}
	}

	return false;
}

bool UNavLinkDefinition::HasAdjustableLinks() const
{
	if (bHasDeterminedAdjustableLinks && !GIsEditor)
	{
		return bHasAdjustableLinks;
	}

	bHasDeterminedAdjustableLinks = true;
	const FNavigationLink* Link = Links.GetTypedData();
	for (int32 LinkIndex = 0; LinkIndex < Links.Num(); ++LinkIndex, ++Link)
	{
		if (Link->MaxFallDownLength > 0)
		{
			bHasAdjustableLinks = true;
			return true;
		}
	}
	const FNavigationSegmentLink* SegmentLink = SegmentLinks.GetTypedData();
	for (int32 LinkIndex = 0; LinkIndex < SegmentLinks.Num(); ++LinkIndex, ++SegmentLink)
	{
		if (SegmentLink->MaxFallDownLength > 0)
		{
			bHasAdjustableLinks = true;
			return true;
		}
	}

	return false;
}



//----------------------------------------------------------------------//
// FAreaNavModifier
//----------------------------------------------------------------------//

FAreaNavModifier::FAreaNavModifier(float Radius, float Height, const FTransform& LocalToWorld, const TSubclassOf<class UNavArea> InAreaClass)
{
	Init(InAreaClass);

	Points.Init(2);
	Points[0] = LocalToWorld.GetLocation();
	Points[1].X = Radius;
	Points[1].Z = Height;
	ShapeType = ENavigationShapeType::Cylinder;

	Bounds = FBox::BuildAABB(LocalToWorld.GetLocation(), FVector(Radius, Radius, Height));
}

FAreaNavModifier::FAreaNavModifier(const FVector& Extent, const FTransform& LocalToWorld, const TSubclassOf<class UNavArea> InAreaClass)
{
	Init(InAreaClass);
	SetBox(FBox::BuildAABB(FVector::ZeroVector, Extent), LocalToWorld);
}

FAreaNavModifier::FAreaNavModifier(const FBox& Box, const FTransform& LocalToWorld, const TSubclassOf<class UNavArea> InAreaClass)
{
	Init(InAreaClass);
	SetBox(Box, LocalToWorld);
}

FAreaNavModifier::FAreaNavModifier(const TArray<FVector>& InPoints, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld, const TSubclassOf<class UNavArea> InAreaClass)
{
	Init(InAreaClass);
	SetConvex(InPoints, 0, InPoints.Num(), CoordType, LocalToWorld);
}

FAreaNavModifier::FAreaNavModifier(const TArray<FVector>& InPoints, const int32 FirstIndex, const int32 LastIndex, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld, const TSubclassOf<class UNavArea> InAreaClass)
{
	Init(InAreaClass);
	SetConvex(InPoints, FirstIndex, LastIndex, CoordType, LocalToWorld);
}

FAreaNavModifier::FAreaNavModifier(const class UBrushComponent* BrushComponent, const TSubclassOf<class UNavArea> InAreaClass)
{
	check(BrushComponent != NULL);

	TArray<FVector> Verts;
	if(BrushComponent->BrushBodySetup)
	{
		for (int32 ElemIndex = 0; ElemIndex < BrushComponent->BrushBodySetup->AggGeom.ConvexElems.Num(); ElemIndex++)
		{
			const FKConvexElem& Convex = BrushComponent->BrushBodySetup->AggGeom.ConvexElems[ElemIndex];
			for (int32 VertexIndex = 0; VertexIndex < Convex.VertexData.Num(); VertexIndex++)
			{
				Verts.AddUnique(Convex.VertexData[VertexIndex]);
			}
		}
	}

	Init(InAreaClass);
	SetConvex(Verts, 0, Verts.Num(), ENavigationCoordSystem::Unreal, BrushComponent->ComponentToWorld);
}

void FAreaNavModifier::GetCylinder(FCylinderNavAreaData& Data) const
{
	check(Points.Num() == 2 && ShapeType == ENavigationShapeType::Cylinder);
	Data.Origin = Points[0];
	Data.Radius = Points[1].X;
	Data.Height = Points[1].Z;
}

void FAreaNavModifier::GetBox(FBoxNavAreaData& Data) const
{
	check(Points.Num() == 2 && ShapeType == ENavigationShapeType::Box);
	Data.Origin = Points[0];
	Data.Extent = Points[1];
}

void FAreaNavModifier::GetConvex(FConvexNavAreaData& Data) const
{
	check(ShapeType == ENavigationShapeType::Convex);
	Data.Points.Append(Points);
	FVector LastPoint = Data.Points.Pop();
	Data.MinZ = LastPoint.X;
	Data.MaxZ = LastPoint.Y;
}

void FAreaNavModifier::Init(const TSubclassOf<class UNavArea> InAreaClass)
{
	Cost = 0.0f;
	FixedCost = 0.0f;
	SetAreaClass(InAreaClass);
}

void FAreaNavModifier::SetAreaClass(const TSubclassOf<class UNavArea> InAreaClass)
{
	AreaClass = InAreaClass;
	bHasMetaAreas = InAreaClass->IsChildOf(UNavAreaMeta::StaticClass());
}

bool IsAngleMatching(float Angle)
{
	const float AngleThreshold = 1.0f; // degrees
	return (Angle < AngleThreshold) || ((90.0f - Angle) < AngleThreshold);
}

void FAreaNavModifier::SetBox(const FBox& Box, const FTransform& LocalToWorld)
{
	const FVector BoxOrigin = Box.GetCenter();
	const FVector BoxExtent = Box.GetExtent();

	TArray<FVector> Corners;
	for (int32 i = 0; i < 8; i++)
	{
		const FVector Dir((i / 4) % 2 ? 1 : -1, (i / 2) % 2 ? 1 : -1, i % 2 ? 1 : -1);
		Corners.Add(LocalToWorld.TransformPosition(BoxOrigin + BoxExtent * Dir));
	}

	// check if it can be used as AABB
	const FRotator Rotation = LocalToWorld.GetRotation().Rotator();
	const float PitchMod = FMath::Fmod(FMath::Abs(Rotation.Pitch), 90.0f);
	const float YawMod = FMath::Fmod(FMath::Abs(Rotation.Yaw), 90.0f);
	const float RollMod = FMath::Fmod(FMath::Abs(Rotation.Roll), 90.0f);
	if (IsAngleMatching(PitchMod) && IsAngleMatching(YawMod) && IsAngleMatching(RollMod))
	{
		Bounds = FBox(0);
		for (int32 i = 0; i < Corners.Num(); i++)
		{
			Bounds += Corners[i];
		}

		Points.Init(2);
		Points[0] = Bounds.GetCenter();
		Points[1] = Bounds.GetExtent();
		ShapeType = ENavigationShapeType::Box;
	}
	else
	{
		SetConvex(Corners, 0, Corners.Num(), ENavigationCoordSystem::Unreal, FTransform::Identity);
	}
}

void FAreaNavModifier::SetConvex(const TArray<FVector>& InPoints, const int32 FirstIndex, const int32 LastIndex, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld)
{
	FConvexNavAreaData ConvexData;
	ConvexData.MinZ = MAX_FLT;
	ConvexData.MaxZ = -MAX_FLT;

	const int MaxConvexPoints = 8;
	TArray<FVector, TInlineAllocator<MaxConvexPoints>> HullVertices;
	HullVertices.Empty(MaxConvexPoints);

	for (int32 i = FirstIndex; i < LastIndex; i++)
	{
		const FVector Point = (CoordType == ENavigationCoordSystem::Recast) ? Recast2UnrealPoint(InPoints[i]) : InPoints[i];

		FVector TransformedPoint = LocalToWorld.TransformPosition(Point);
		ConvexData.MinZ = FMath::Min( ConvexData.MinZ, TransformedPoint.Z );
		ConvexData.MaxZ = FMath::Max( ConvexData.MaxZ, TransformedPoint.Z );
		TransformedPoint.Z = 0.f;

		// check if there's a similar point already in HullVertices array
		bool bUnique = true;
		const FVector* RESTRICT Start = HullVertices.GetTypedData();
		for (const FVector* RESTRICT Data = Start, * RESTRICT DataEnd = Data + HullVertices.Num(); Data != DataEnd; ++Data)
		{
			if (FVector::DistSquared(*Data, TransformedPoint) < CONVEX_HULL_POINTS_MIN_DISTANCE_SQ)
			{
				bUnique = false;
				break;
			}
		}
		
		if (bUnique)
		{
			HullVertices.Add(TransformedPoint);
		}
	}

	TArray<int32, TInlineAllocator<MaxConvexPoints>> HullIndices;
	HullIndices.Empty(MaxConvexPoints);

	ConvexHull2D::ComputeConvexHull(HullVertices, HullIndices);
	if (HullIndices.Num())
	{
		Bounds = FBox(0);
		for(int32 i = 0; i < HullIndices.Num(); ++i)
		{
			const FVector& HullVert = HullVertices[HullIndices[i]];
			ConvexData.Points.Add(HullVert);
			Bounds += HullVert;
		}

		Bounds.Min.Z = ConvexData.MinZ;
		Bounds.Max.Z = ConvexData.MaxZ;

		Points.Append(ConvexData.Points);
		Points.Add(FVector(ConvexData.MinZ, ConvexData.MaxZ, 0));
		ShapeType = ENavigationShapeType::Convex;
	}
	else
	{
		ShapeType = ENavigationShapeType::Unknown;
	}
}

//----------------------------------------------------------------------//
// FCustomLinkNavModifier
//----------------------------------------------------------------------//
void FCustomLinkNavModifier::Set(TSubclassOf<class UNavLinkDefinition> InPresetLinkClass, const FTransform& InLocalToWorld)
{
	LinkDefinitionClass = InPresetLinkClass;
	LocalToWorld = InLocalToWorld;
	bHasMetaAreas = false;

	const TArray<FNavigationLink>& NavLinks = UNavLinkDefinition::GetLinksDefinition(InPresetLinkClass);
	for (int32 i = 0; i < NavLinks.Num() && !bHasMetaAreas; i++)
	{
		bHasMetaAreas = NavLinks[i].AreaClass->IsChildOf(UNavAreaMeta::StaticClass());
	}
}

//----------------------------------------------------------------------//
// FSimpleLinkNavModifier
//----------------------------------------------------------------------//
void FSimpleLinkNavModifier::SetLinks(const TArray<FNavigationLink>& InLinks)
{
	Links = InLinks;
	bHasMetaAreasPoint = false;

	for (int32 i = 0; i < InLinks.Num() && (!bHasMetaAreasPoint || !bHasFallDownLinks); i++)
	{
		bHasMetaAreasPoint |= InLinks[i].AreaClass->IsChildOf(UNavAreaMeta::StaticClass());
		bHasFallDownLinks |= InLinks[i].MaxFallDownLength > 0.f;
	}

	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}

void FSimpleLinkNavModifier::SetSegmentLinks(const TArray<FNavigationSegmentLink>& InLinks)
{
	SegmentLinks = InLinks;

	bHasMetaAreasSegment = false;
	for (int32 i = 0; i < SegmentLinks.Num(); i++)
	{
		SegmentLinks[i].UserId = UserId;
		bHasMetaAreasSegment |= InLinks[i].AreaClass->IsChildOf(UNavAreaMeta::StaticClass());
		bHasFallDownLinks |= InLinks[i].MaxFallDownLength > 0.f;
	}
	
	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}

void FSimpleLinkNavModifier::AppendLinks(const TArray<FNavigationLink>& InLinks)
{
	Links.Append(InLinks);

	for (int32 i = 0; i < InLinks.Num() && (!bHasMetaAreasPoint || !bHasFallDownLinks); i++)
	{
		bHasMetaAreasPoint |= InLinks[i].AreaClass->IsChildOf(UNavAreaMeta::StaticClass());
		bHasFallDownLinks |= InLinks[i].MaxFallDownLength > 0.f;
	}

	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}

void FSimpleLinkNavModifier::AppendSegmentLinks(const TArray<FNavigationSegmentLink>& InLinks)
{
	const int32 LinkBase = SegmentLinks.Num();
	SegmentLinks.Append(InLinks);

	for (int32 i = 0; i < InLinks.Num(); i++)
	{
		SegmentLinks[LinkBase + i].UserId = UserId;
		bHasMetaAreasPoint |= InLinks[i].AreaClass->IsChildOf(UNavAreaMeta::StaticClass());
		bHasFallDownLinks |= InLinks[i].MaxFallDownLength > 0.f;
	}

	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}

void FSimpleLinkNavModifier::AddLink(const FNavigationLink& InLink)
{
	Links.Add(InLink);

	bHasMetaAreasPoint = InLink.AreaClass->IsChildOf(UNavAreaMeta::StaticClass());
	bHasFallDownLinks |= InLink.MaxFallDownLength > 0.f;
	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}

void FSimpleLinkNavModifier::AddSegmentLink(const FNavigationSegmentLink& InLink)
{
	const int32 LinkBase = SegmentLinks.Num();
	SegmentLinks.Add(InLink);
	SegmentLinks[LinkBase].UserId = UserId;

	bHasMetaAreasSegment = InLink.AreaClass->IsChildOf(UNavAreaMeta::StaticClass());
	bHasFallDownLinks |= InLink.MaxFallDownLength > 0.f;
	bHasMetaAreas = bHasMetaAreasSegment || bHasMetaAreasPoint;
}


//----------------------------------------------------------------------//
// FCompositeNavMeshModifier
//----------------------------------------------------------------------//
void FCompositeNavModifier::Reset()
{
	Areas.Reset();
	SimpleLinks.Reset();
	CustomLinks.Reset();
}

void FCompositeNavModifier::Empty()
{
	Areas.Empty();
	SimpleLinks.Empty();
	CustomLinks.Empty();
	bHasPotentialLinks = false;
}

FCompositeNavModifier FCompositeNavModifier::GetInstantiatedMetaModifier(const FNavAgentProperties* NavAgent, TWeakObjectPtr<UObject> WeakOwnerPtr) const
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_MetaAreaTranslation);
	FCompositeNavModifier Result;

	check(NavAgent);
	// should not be called when HasMetaAreas == false since it's a waste of performance
	ensure(HasMetaAreas() == true);

	UObject* ObjectOwner = WeakOwnerPtr.Get();
	if (ObjectOwner == NULL)
	{
		return Result;
	}

	const AActor* ActorOwner = Cast<UNavigationProxy>(ObjectOwner) ? ((UNavigationProxy*)ObjectOwner)->MyOwner :
		Cast<AActor>(ObjectOwner) ? (AActor*)ObjectOwner : Cast<AActor>(ObjectOwner->GetOuter());

	if (ActorOwner == NULL)
	{
		return Result;
	}

	// copy values
	Result = *this;

	{
		FAreaNavModifier* Area = Result.Areas.GetTypedData();
		for (int32 Index = 0; Index < Result.Areas.Num(); ++Index, ++Area)
		{
			if (Area->HasMetaAreas())
			{
				Area->SetAreaClass(UNavAreaMeta::PickAreaClass(Area->GetAreaClass(), ActorOwner, *NavAgent));
			}
		}
	}

	{
		FSimpleLinkNavModifier* SimpleLink = Result.SimpleLinks.GetTypedData();
		for (int32 Index = 0; Index < Result.SimpleLinks.Num(); ++Index, ++SimpleLink)
		{
			if (SimpleLink->HasMetaAreas())
			{
				for (int32 LinkIndex = 0; LinkIndex < SimpleLink->Links.Num(); ++LinkIndex)
				{
					FNavigationLink& Link = SimpleLink->Links[LinkIndex];
					Link.AreaClass = UNavAreaMeta::PickAreaClass(Link.AreaClass, ActorOwner, *NavAgent);
				}

				for (int32 LinkIndex = 0; LinkIndex < SimpleLink->SegmentLinks.Num(); ++LinkIndex)
				{
					FNavigationSegmentLink& Link = SimpleLink->SegmentLinks[LinkIndex];
					Link.AreaClass = UNavAreaMeta::PickAreaClass(Link.AreaClass, ActorOwner, *NavAgent);
				}
			}
		}
	}

	{
		// create new entry in CustomLinks, and put there all regular links that have meta area class
		// making plain FNavigationLink from them first
		Result.SimpleLinks.Reserve(Result.CustomLinks.Num() + Result.SimpleLinks.Num());
		
		for (int32 Index = Result.CustomLinks.Num() - 1; Index >= 0; --Index)
		{
			FCustomLinkNavModifier* CustomLink = &Result.CustomLinks[Index];
			if (CustomLink->HasMetaAreas())
			{
				const TArray<FNavigationLink>& Links = UNavLinkDefinition::GetLinksDefinition(CustomLink->GetNavLinkClass());

				FSimpleLinkNavModifier& SimpleLink = Result.SimpleLinks[Result.SimpleLinks.AddZeroed(1)];
				SimpleLink.LocalToWorld = CustomLink->LocalToWorld;
				SimpleLink.Links.Reserve(Links.Num());

				// and copy all links to FCompositeNavMeshModifier::CustomLinks 
				// updating AreaClass if it's meta area
				for (int32 LinkIndex = 0; LinkIndex < Links.Num(); ++LinkIndex)
				{
					const int32 AddedIdx = SimpleLink.Links.Add(Links[LinkIndex]);
					FNavigationLink& NavLink = SimpleLink.Links[AddedIdx];
					NavLink.AreaClass = UNavAreaMeta::PickAreaClass(NavLink.AreaClass, ActorOwner, *NavAgent);
				}				

				const TArray<FNavigationSegmentLink>& SegmentLinks = UNavLinkDefinition::GetSegmentLinksDefinition(CustomLink->GetNavLinkClass());

				FSimpleLinkNavModifier& SimpleSegLink = Result.SimpleLinks[Result.SimpleLinks.AddZeroed(1)];
				SimpleSegLink.LocalToWorld = CustomLink->LocalToWorld;
				SimpleSegLink.SegmentLinks.Reserve(SegmentLinks.Num());

				// and copy all links to FCompositeNavMeshModifier::CustomLinks 
				// updating AreaClass if it's meta area
				for (int32 LinkIndex = 0; LinkIndex < SegmentLinks.Num(); ++LinkIndex)
				{
					const int32 AddedIdx = SimpleSegLink.SegmentLinks.Add(SegmentLinks[LinkIndex]);
					FNavigationSegmentLink& NavLink = SimpleSegLink.SegmentLinks[AddedIdx];
					NavLink.AreaClass = UNavAreaMeta::PickAreaClass(NavLink.AreaClass, ActorOwner, *NavAgent);
				}

				Result.CustomLinks.RemoveAtSwap(Index, 1, false);
			}
		}
	}

	return Result;
}

uint32 FCompositeNavModifier::GetAllocatedSize() const
{
	uint32 MemUsed = Areas.GetAllocatedSize() + SimpleLinks.GetAllocatedSize() + CustomLinks.GetAllocatedSize();

	const FSimpleLinkNavModifier* SimpleLink = SimpleLinks.GetTypedData();
	for (int32 Index = 0; Index < SimpleLinks.Num(); ++Index, ++SimpleLink)
	{
		MemUsed += SimpleLink->Links.GetAllocatedSize();
	}

	return MemUsed;
}

//----------------------------------------------------------------------//
// UNavLinkTrivial
//----------------------------------------------------------------------//
UNavLinkTrivial::UNavLinkTrivial(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	FNavigationLink& Link = Links[Links.Add(FNavigationLink(FVector(0,100,0), FVector(0,-100,0)))];
}
