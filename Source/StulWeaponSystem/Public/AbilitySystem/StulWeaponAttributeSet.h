// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "StulWeaponAttributeSet.generated.h"

class FLifetimeProperty;
struct FGameplayEffectModCallbackData;

#define STUL_WEAPON_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** Runtime attributes owned by a weapon Ability System Component. */
UCLASS()
class STULWEAPONSYSTEM_API UStulWeaponAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, CurrentAmmo)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, MaxAmmo)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, FireCost)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, BurstShotCount)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, BurstInterval)

	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, ShotsPerFire)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, PatternScale)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, MinSpread)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, AirMinSpread)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, MaxSpread)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, CrouchSpreadMultiplier)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, StandSpreadMultiplier)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, AirSpreadMultiplier)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, SpreadIncreasePerShot)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, MaxSpreadOvershoot)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, SpreadInterpSpeed)

	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, Damage)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, ReloadDuration)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, AimDuration)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, FireModeChangeDuration)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, FireInterval)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, MaxPenetrationCount)
	STUL_WEAPON_ATTRIBUTE_ACCESSORS(UStulWeaponAttributeSet, MaxPenetrationStrength)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	void ClampAttributeValue(const FGameplayAttribute& Attribute, float& NewValue) const;

	UFUNCTION()
	void OnRep_CurrentAmmo(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxAmmo(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_FireCost(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_BurstShotCount(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_BurstInterval(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_ShotsPerFire(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_PatternScale(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MinSpread(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AirMinSpread(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxSpread(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_CrouchSpreadMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_StandSpreadMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AirSpreadMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_SpreadIncreasePerShot(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxSpreadOvershoot(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_SpreadInterpSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Damage(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_ReloadDuration(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AimDuration(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_FireModeChangeDuration(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_FireInterval(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxPenetrationCount(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxPenetrationStrength(const FGameplayAttributeData& OldValue);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentAmmo, Category = "Weapon|Ammo", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData CurrentAmmo = 0.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxAmmo, Category = "Weapon|Ammo", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxAmmo = 0.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FireCost, Category = "Weapon|Ammo", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData FireCost = 1.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BurstShotCount, Category = "Weapon|Fire", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData BurstShotCount = 3.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BurstInterval, Category = "Weapon|Fire", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData BurstInterval = 0.2f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShotsPerFire, Category = "Weapon|Fire", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ShotsPerFire = 1.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PatternScale, Category = "Weapon|Pattern", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData PatternScale = 1.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MinSpread, Category = "Weapon|Spread", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MinSpread = 0.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AirMinSpread, Category = "Weapon|Spread", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData AirMinSpread = 7.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSpread, Category = "Weapon|Spread", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxSpread = 25.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CrouchSpreadMultiplier, Category = "Weapon|Spread", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData CrouchSpreadMultiplier = 0.25f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StandSpreadMultiplier, Category = "Weapon|Spread", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData StandSpreadMultiplier = 0.45f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AirSpreadMultiplier, Category = "Weapon|Spread", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData AirSpreadMultiplier = 1.5f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpreadIncreasePerShot, Category = "Weapon|Spread", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData SpreadIncreasePerShot = 2.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSpreadOvershoot, Category = "Weapon|Spread", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxSpreadOvershoot = 4.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpreadInterpSpeed, Category = "Weapon|Spread", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData SpreadInterpSpeed = 5.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Damage, Category = "Weapon|Damage", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Damage = 18.0f;

	/** Full reload duration, or time between inserts in Custom mode. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReloadDuration, Category = "Weapon|Reload", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ReloadDuration = 1.0f;

	/** Local presentation transition duration requested when entering or leaving aim. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AimDuration, Category = "Weapon|Aim", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData AimDuration = 0.2f;

	/** Delay before a requested fire mode becomes active. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FireModeChangeDuration, Category = "Weapon|Fire Mode", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData FireModeChangeDuration = 0.25f;

	/** Time in seconds between consecutive shots. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FireInterval, Category = "Weapon|Fire", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData FireInterval = 0.1f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxPenetrationCount, Category = "Weapon|Penetration", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxPenetrationCount = 1.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxPenetrationStrength, Category = "Weapon|Penetration", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxPenetrationStrength = 0.0f;
};

#undef STUL_WEAPON_ATTRIBUTE_ACCESSORS
