#pragma once
#include <array>
namespace Settings {
enum Key { Brightness, FirstPersonScale, CoreBrightness, ReadyPulse, AssemblySnap, Recoil, CancelScatter, FlightMotes, BrokenRings, ImpactEmbers, IceCrystals, ResidueDuration, IceSound, FireDamage, FireDamageAmount, TransformKey, Count };
inline constexpr std::array<float,Count> Defaults{1.0f,0.8f,0.45f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,5.0f,1.0f,0.0f,3.0f,34.0f};
inline constexpr std::array<float,Count> Minimum{0.25f,0.5f,0.1f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,3.0f,0.0f,0.0f,1.0f,0.0f};
inline constexpr std::array<float,Count> Maximum{2.0f,1.2f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,8.0f,1.0f,1.0f,10.0f,255.0f};
inline constexpr std::array<unsigned int,Count> FormIDs{0x921,0x922,0x923,0x924,0x925,0x926,0x927,0x92a,0x92b,0x92c,0x92d,0x92e,0x931,0x932,0x933,0x944};
inline constexpr char ModuleName[]="AstralSigil.esp";
}
