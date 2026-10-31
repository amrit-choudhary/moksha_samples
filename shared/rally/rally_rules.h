/**
 * Rally court and rules, shared by the rally sample and its server so both clamp and draw the same court.
 * Court coordinates: the origin is the centre, Y up, seat 0 defends the bottom and seat 1 the top.
 */
#pragma once

#include <cstdint>

namespace Samples::Rally {

enum class Phase : uint8_t {
    Waiting,  // fewer than two seated players
    Serve,    // ball held in the centre before a serve
    Play,     // ball in play
    Won,      // a player reached WIN_SCORE; a new match starts shortly
};

constexpr uint8_t NO_CLIENT = 0xFF;  // an empty seat

constexpr float COURT_HALF_WIDTH = 45.0f;
constexpr float COURT_HALF_HEIGHT = 80.0f;
constexpr float PADDLE_HALF_WIDTH = 9.0f;
constexpr float PADDLE_HALF_HEIGHT = 1.5f;
constexpr float PADDLE_Y = 70.0f;  // distance of each paddle's centre from the court's centre line
constexpr float BALL_HALF_SIZE = 1.5f;
constexpr uint8_t WIN_SCORE = 7;

// The furthest a paddle's centre can go from the middle, so it stays inside the side walls.
constexpr float PADDLE_MAX_X = COURT_HALF_WIDTH - PADDLE_HALF_WIDTH;

}  // namespace Samples::Rally
