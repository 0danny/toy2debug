#pragma once

#include "Hook.hpp"
#include "ToyTypes.hpp"

#include <windows.h>

namespace DX6
{
	class DeviceHooks : public Hook
	{
	public:
		DeviceHooks()
			: Hook("DeviceHooks") {};

		bool init() override;

		static bool initialize(HWND hWnd, bool fullscreen, int32_t width, int32_t height, GUID* ddGuid, const IID& deviceGuid);

	private:
		static bool createDirectDrawInterface(GUID* ddGuid, bool fullscreen);
		static bool createD3DInterface();
		static bool selectDevice(const IID& deviceGuid);
		static bool createSurfaces(bool fullscreen, int32_t width, int32_t height);
		static bool createZBuffer();
		static bool createDevice(const IID& deviceGuid);
		static bool createViewport();
		static void cleanup();
		static void initSurfaceDesc(LPDDSURFACEDESC2 desc, uint32_t flags, uint32_t caps);

		static HRESULT CALLBACK enumZBufferCallback(LPDDPIXELFORMAT format, LPVOID context);
	};
}
