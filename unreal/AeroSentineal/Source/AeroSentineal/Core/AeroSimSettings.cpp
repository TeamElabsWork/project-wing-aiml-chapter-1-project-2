// Copyright AeroSentinel. All Rights Reserved.

#include "Core/AeroSimSettings.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

UAeroSimSettings::UAeroSimSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("AeroSentinel Simulation");
}

FString UAeroSimSettings::GetAbsoluteDatasetRoot() const
{
	// FPaths::ProjectDir() is itself relative to the engine binaries directory in an editor
	// process, so ConvertRelativePathToFull alone leaves "../../.." in the result. Ask the file
	// manager for a real rooted path instead - the dataset writer needs one.
	const FString Combined = FPaths::Combine(FPaths::ProjectDir(), DatasetRootDirectory);
	return IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Combined);
}

FDateTime UAeroSimSettings::GetStartUtc() const
{
	// FDateTime clamps out-of-range fields internally, so a bad ini edit degrades to a valid date
	// instead of asserting.
	return FDateTime(StartYear, StartMonth, StartDay, StartHourUtc, 0, 0);
}
