// UNPAID: OVERTIME - shared automation test settings.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

/** Flags shared by every UnpaidOvertime.Core.* unit test (pure logic, editor context). */
#define UO_CORE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
