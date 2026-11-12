/**
 * Obstacle outlines for the wind tunnel, in domain units around the pivot the obstacle is dragged and turned by.
 * Airfoils come from NACA codes: 4 digits MPTT (camber M%, at P tenths of the chord, thickness TT%) or 5 digits LPQTT
 * (standard series, Q = 0: design lift 0.15 L, camber peak at P/20 of the chord), sampled with cosine spacing so the
 * nose is smooth, and pivoting at the quarter chord. The aircraft and car are simple made-up side silhouettes.
 */

#pragma once

#include "wind_tunnel/game/wind_tunnel_sim.h"

namespace Samples::WindTunnelShapes {

using Outline = WindTunnelSim::Outline;

Outline Circle(float radius);
Outline Square(float halfSide);
Outline Plate(float halfLength, float halfThickness);

// False, leaving out untouched, if the code isn't a 4-digit or standard 5-digit NACA code.
bool Naca(const char* code, float chord, Outline& out);

// Nose to the left (into the wind), centred on its bounding box.
Outline Aircraft(float length);
Outline Car(float length);

}  // namespace Samples::WindTunnelShapes
