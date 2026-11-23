// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/** Single log category for every AeroSentinel subsystem, so simulation behaviour can be
 *  grepped in isolation from engine and editor noise. */
DECLARE_LOG_CATEGORY_EXTERN(LogAeroSentinel, Log, All);
