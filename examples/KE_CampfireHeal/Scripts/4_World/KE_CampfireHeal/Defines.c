// =============================================================================
//  Defines.c  —  Compile-time configuration for KE_CampfireHeal
// =============================================================================
//
//  These are static `const` instead of #defines because Enforce Script doesn't
//  let you reference #define values inside class bodies cleanly. They're
//  treated as compile-time constants by the compiler.
// =============================================================================

class KE_CampfireHealConfig
{
    // ---- Healing zone ----
    static const float HEAL_RADIUS_M       = 3.0;     // metres around fireplace
    static const float TICK_INTERVAL_S     = 1.0;     // seconds between ticks

    // ---- Per-tick effect ----
    static const float HEALTH_PER_TICK     = 1.5;     // HP regen
    static const float BLOOD_PER_TICK      = 5.0;     // blood regen
    static const float HEAT_BUFFER_PER_TICK = 0.0;    // 0 disables — vanilla
                                                       // fireplace already heats

    // ---- Behaviour ----
    static const bool  REQUIRE_BURNING     = true;    // false = any fireplace
    static const bool  REQUIRE_ALIVE       = true;    // skip dead/unconscious
};
