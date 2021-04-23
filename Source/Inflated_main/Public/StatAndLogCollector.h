// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <string>

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamStats.h"
#include "StatAndLogCollector.generated.h"

/**
 * 
 */
UCLASS()
class INFLATED_MAIN_API UStatAndLogCollector : public UBlueprintFunctionLibrary
{	
	GENERATED_BODY()
	UFUNCTION(BlueprintCallable, Category="SteamStats")
		static bool SendStatToSteam(FString SteamStat_Name);
};
