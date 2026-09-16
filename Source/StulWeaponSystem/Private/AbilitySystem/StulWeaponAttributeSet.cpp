// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/StulWeaponAttributeSet.h"

#include "GameFramework/Actor.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

void UStulWeaponAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

#define STUL_REPLICATE_ATTRIBUTE(PropertyName) \
	DOREPLIFETIME_CONDITION_NOTIFY(UStulWeaponAttributeSet, PropertyName, COND_OwnerOnly, REPNOTIFY_Always)

	STUL_REPLICATE_ATTRIBUTE(CurrentAmmo);
	STUL_REPLICATE_ATTRIBUTE(MaxAmmo);
	STUL_REPLICATE_ATTRIBUTE(FireCost);
	STUL_REPLICATE_ATTRIBUTE(BurstShotCount);
	STUL_REPLICATE_ATTRIBUTE(BurstInterval);
	STUL_REPLICATE_ATTRIBUTE(ShotsPerFire);
	STUL_REPLICATE_ATTRIBUTE(PatternScale);
	STUL_REPLICATE_ATTRIBUTE(MinSpread);
	STUL_REPLICATE_ATTRIBUTE(AirMinSpread);
	STUL_REPLICATE_ATTRIBUTE(MaxSpread);
	STUL_REPLICATE_ATTRIBUTE(CrouchSpreadMultiplier);
	STUL_REPLICATE_ATTRIBUTE(StandSpreadMultiplier);
	STUL_REPLICATE_ATTRIBUTE(AirSpreadMultiplier);
	STUL_REPLICATE_ATTRIBUTE(SpreadIncreasePerShot);
	STUL_REPLICATE_ATTRIBUTE(MaxSpreadOvershoot);
	STUL_REPLICATE_ATTRIBUTE(SpreadInterpSpeed);
	STUL_REPLICATE_ATTRIBUTE(Damage);
	STUL_REPLICATE_ATTRIBUTE(ReloadDuration);
	STUL_REPLICATE_ATTRIBUTE(AimDuration);
	STUL_REPLICATE_ATTRIBUTE(FireModeChangeDuration);
	STUL_REPLICATE_ATTRIBUTE(FireInterval);
	STUL_REPLICATE_ATTRIBUTE(MaxPenetrationCount);
	STUL_REPLICATE_ATTRIBUTE(MaxPenetrationStrength);

#undef STUL_REPLICATE_ATTRIBUTE
}

void UStulWeaponAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttributeValue(Attribute, NewValue);
}

void UStulWeaponAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttributeValue(Attribute, NewValue);
}

void UStulWeaponAttributeSet::PostAttributeChange(
	const FGameplayAttribute& Attribute,
	const float OldValue,
	const float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	const AActor* OwningActor = GetOwningActor();
	if (Attribute == GetMaxAmmoAttribute()
		&& GetCurrentAmmo() > NewValue
		&& OwningActor
		&& OwningActor->HasAuthority())
	{
		SetCurrentAmmo(NewValue);
	}
}

void UStulWeaponAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetCurrentAmmoAttribute())
	{
		SetCurrentAmmo(FMath::Clamp(GetCurrentAmmo(), 0.0f, GetMaxAmmo()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxAmmoAttribute())
	{
		SetMaxAmmo(FMath::Max(0.0f, GetMaxAmmo()));
		SetCurrentAmmo(FMath::Clamp(GetCurrentAmmo(), 0.0f, GetMaxAmmo()));
	}
}

void UStulWeaponAttributeSet::ClampAttributeValue(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetCurrentAmmoAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxAmmo());
	}
	else if (Attribute == GetMaxAmmoAttribute() ||
		Attribute == GetFireCostAttribute() ||
		Attribute == GetBurstIntervalAttribute() ||
		Attribute == GetPatternScaleAttribute() ||
		Attribute == GetMinSpreadAttribute() ||
		Attribute == GetAirMinSpreadAttribute() ||
		Attribute == GetMaxSpreadAttribute() ||
		Attribute == GetSpreadIncreasePerShotAttribute() ||
		Attribute == GetMaxSpreadOvershootAttribute() ||
		Attribute == GetSpreadInterpSpeedAttribute() ||
		Attribute == GetDamageAttribute() ||
		Attribute == GetReloadDurationAttribute() ||
		Attribute == GetAimDurationAttribute() ||
		Attribute == GetFireModeChangeDurationAttribute() ||
		Attribute == GetFireIntervalAttribute() ||
		Attribute == GetMaxPenetrationCountAttribute() ||
		Attribute == GetMaxPenetrationStrengthAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
	else if (Attribute == GetBurstShotCountAttribute() || Attribute == GetShotsPerFireAttribute())
	{
		NewValue = FMath::Max(1.0f, NewValue);
	}
	else if (Attribute == GetCrouchSpreadMultiplierAttribute() || Attribute == GetStandSpreadMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
	}
	else if (Attribute == GetAirSpreadMultiplierAttribute())
	{
		NewValue = FMath::Max(1.0f, NewValue);
	}
}

#define STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(PropertyName) \
	void UStulWeaponAttributeSet::OnRep_##PropertyName(const FGameplayAttributeData& OldValue) \
	{ \
		GAMEPLAYATTRIBUTE_REPNOTIFY(UStulWeaponAttributeSet, PropertyName, OldValue); \
	}

STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(CurrentAmmo)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(MaxAmmo)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(FireCost)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(BurstShotCount)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(BurstInterval)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(ShotsPerFire)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(PatternScale)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(MinSpread)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(AirMinSpread)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(MaxSpread)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(CrouchSpreadMultiplier)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(StandSpreadMultiplier)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(AirSpreadMultiplier)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(SpreadIncreasePerShot)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(MaxSpreadOvershoot)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(SpreadInterpSpeed)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(Damage)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(ReloadDuration)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(AimDuration)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(FireModeChangeDuration)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(FireInterval)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(MaxPenetrationCount)
STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY(MaxPenetrationStrength)

#undef STUL_IMPLEMENT_ATTRIBUTE_REP_NOTIFY
