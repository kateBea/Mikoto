//
// Created by kate on 11/1/25.
//

/**
 * @file Core.hh
 * @brief Mikoto Core module.
 *
 * Core-related headers in Mikoto.
 */

#ifndef MIKOTO_CORE_HH
#define MIKOTO_CORE_HH

// ===========================================================
 // Core Utilities
// ===========================================================
#include <Core/ArgsParser.hh>
#include <Core/Configuration.hh>
#include <Core/ExecuteProcess.hh>
#include <Core/Platform.hh>
#include <Core/Serializer.hh>
#include <Core/SystemStats.hh>
#include <Core/TimeService.hh>
#include <Core/Timer.hh>

// ===========================================================
 // Event System
// ===========================================================
#include <Core/Event.hh>
#include <Core/CoreEvents.hh>
#include <Core/EventService.hh>

// ===========================================================
 // Input System
// ===========================================================
#include <Core/InputService.hh>
#include <Core/KeyCodes.hh>
#include <Core/MouseCodes.hh>

// ===========================================================
 // Layers and Application
// ===========================================================
#include <Core/LayerStack.hh>
#include <Core/Root.hh>

// ===========================================================
 // Utilities
// ===========================================================
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>

#endif // MIKOTO_CORE_HH
