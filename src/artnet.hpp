#pragma once

#define NOMINMAX  // Disable Windows min/max macros

#include <iostream>
#include <cstdint>
#include <array>
#include <ranges>
#include <chrono>

#include <winsock2.h>

// DMX's full name is DMX512
// VRSL only supports 3 universes skipping 8 channels per universe

namespace ArtNet {
	constexpr int DMX_UNIVERSE_SIZE = 512;
	constexpr int VRSL_MAX_UNIVERSES = 3;
	constexpr int VRSL_UNIVERSE_GRID = (DMX_UNIVERSE_SIZE + 8);
	constexpr int TOTAL_DMX_CHANNELS = VRSL_UNIVERSE_GRID * VRSL_MAX_UNIVERSES;

	class UniverseLogger {
	private:
		std::array<std::chrono::steady_clock::time_point, VRSL_MAX_UNIVERSES> networkTimeRecord = {};
		std::array<std::chrono::duration<double>, VRSL_MAX_UNIVERSES> networkTimeDelta = {};

	public:
		void MeasureTimeDelta(uint8_t universeID);
		auto GetTimeDeltasMs();
	};
}

SOCKET setupArtNetSocket(int port);
void receiveArtNetData(SOCKET sock, std::array<uint8_t, ArtNet::TOTAL_DMX_CHANNELS>& dmxData, ArtNet::UniverseLogger& logger);