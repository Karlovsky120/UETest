// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "SkillSystemModulePrivatePCH.h"

UGameplayEffectExtension_LifestealTest::UGameplayEffectExtension_LifestealTest(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	HealthRestoreGameplayEffect = NULL;
}


void UGameplayEffectExtension_LifestealTest::PreGameplayEffectExecute(const FGameplayModifierEvaluatedData &SelfData, FGameplayEffectModCallbackData &Data) const
{

}

void UGameplayEffectExtension_LifestealTest::PostGameplayEffectExecute(const FGameplayModifierEvaluatedData &SelfData, const FGameplayEffectModCallbackData &Data) const
{

	float DamageDone = Data.EvaluatedData.Magnitude;
	float LifestealPCT = SelfData.Magnitude;

	float HealthToRestore = -DamageDone * LifestealPCT;
	if (HealthToRestore > 0.f)
	{
		UAttributeComponent *Source = Data.EffectSpec.InstigatorStack.GetOriginInstigatorAttributeComponent();

		UGameplayEffect * LocalHealthRestore = HealthRestoreGameplayEffect;
		if (!LocalHealthRestore)
		{
			UProperty *HealthProperty = FindFieldChecked<UProperty>(USkillSystemTestAttributeSet::StaticClass(), GET_MEMBER_NAME_CHECKED(USkillSystemTestAttributeSet, Health));

			// Since this is a test class and we don't want to tie it any actual content assets, just construct a GameplayEffect here.
			LocalHealthRestore = Cast<UGameplayEffect>(StaticConstructObject(UGameplayEffect::StaticClass(), GetTransientPackage(), FName(TEXT("LifestealHealthRestore"))));
			LocalHealthRestore->Modifiers.SetNum(1);
			LocalHealthRestore->Modifiers[0].Magnitude.SetValue(HealthToRestore);
			LocalHealthRestore->Modifiers[0].ModifierType = EGameplayMod::Attribute;
			LocalHealthRestore->Modifiers[0].ModifierOp = EGameplayModOp::Additive;
			LocalHealthRestore->Modifiers[0].Attribute.SetUProperty(HealthProperty);
			LocalHealthRestore->Modifiers[0].OwnedTags.AddTag(FName(TEXT("Lifesteal")));
			LocalHealthRestore->Duration.Value = UGameplayEffect::INSTANT_APPLICATION;
			LocalHealthRestore->Period.Value = UGameplayEffect::NO_PERIOD;
		}

		if (SelfData.Handle.IsValid())
		{
			// We are coming from an active gameplay effect
			check(SelfData.Handle.IsValid());
		}

		// Apply a GameplayEffect to restore health
		// We make the GameplayEffect's level = the health restored. This is one approach. We could also
		// try a basic restore 1 health item but apply a 2nd GE to modify that - but that seems like too many levels of indirection
		Source->ApplyGameplayEffectToSelf(LocalHealthRestore, HealthToRestore, Source->GetOwner());
	}
}