// ImGui - standalone example application for DirectX 11
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
#define NOMINMAX  
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#include <iostream>
#include "OVR_Platform.h"
#include "OVR_CAPI.h"
#include "Extras/OVR_Math.h"
#include <vector>
#include <algorithm>

#pragma comment(lib,"LibOVR.lib")

// Data
static ID3D11Device*            g_pd3dDevice = NULL;
static ID3D11DeviceContext*     g_pd3dDeviceContext = NULL;
static IDXGISwapChain*          g_pSwapChain = NULL;
static ID3D11RenderTargetView*  g_mainRenderTargetView = NULL;
ovrSession g_HMD = 0;

class AABB
{
public:
	OVR::Vector3f minCorner;
	OVR::Vector3f maxCorner;

	inline AABB() :minCorner(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()), maxCorner(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min())
	{
	}

	inline AABB(const OVR::Vector3f &v) : minCorner(v), maxCorner(v)
	{
	}

	static OVR::Vector3f minimum(const OVR::Vector3f &a, const OVR::Vector3f &b)
	{
		return OVR::Vector3f(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
	}

	static OVR::Vector3f maximum(const OVR::Vector3f &a, const OVR::Vector3f &b)
	{
		return OVR::Vector3f(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
	}

	inline AABB &merge(const OVR::Vector3f &v)
	{
		minCorner = minimum(minCorner, v);
		maxCorner = maximum(maxCorner, v);
		return *this;
	}

	inline float width() const
	{
		return (maxCorner.x - minCorner.x);
	}

	inline float height() const
	{
		return (maxCorner.y - minCorner.y);
	}

	inline float length() const
	{
		return (maxCorner.z - minCorner.z);
	}

	inline OVR::Vector3f size() const
	{
		return (maxCorner - minCorner);
	}

	inline OVR::Vector3f centre() const
	{
		return (maxCorner + minCorner)*0.5;
	}
};

bool g_minimised = false;

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

HRESULT CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
		return E_FAIL;

	CreateRenderTarget();

	return S_OK;
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;
	std::cout << msg << std::endl;
	switch (msg)
	{
	case WM_PAINT:
		g_minimised = false;
		break;
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
		{
			g_minimised = true;
		}
		else
		{
			if (g_pd3dDevice != NULL)
			{
				ImGui_ImplDX11_InvalidateDeviceObjects();
				CleanupRenderTarget();
				g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
				CreateRenderTarget();
				ImGui_ImplDX11_CreateDeviceObjects();
			}
		}
		if (wParam == SIZE_RESTORED)
		{
			g_minimised = false;
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void appendstring(std::string &s, const std::string &a)
{
	if (s != "")
	{
		s += ", ";
	}
	s += a;
}

template<typename T>
inline T remap(const T &src_range_start, const T &src_range_end, const T &dst_range_start, const T &dst_range_end, const T &value_to_remap)
{
	return ((dst_range_end - dst_range_start) * (value_to_remap - src_range_start)) / (src_range_end - src_range_start) + dst_range_start;
}

ImVec2 remap(const AABB &aabb, const ImVec2 &size, const ImVec2 &offset, const OVR::Vector3f &p)
{
	ImVec2 out;
	float aspectOculus = aabb.width() / aabb.length();
	float aspectImGui = size.x / size.y;
	if (aspectOculus > aspectImGui)
	{
		out.x = remap<float>(aabb.minCorner.x, aabb.maxCorner.x, 0, size.x, p.x) + offset.x;
		out.y = remap<float>(aabb.minCorner.z, aabb.maxCorner.z, 0, size.x / aspectOculus, p.z) + (size.y - (size.x / aspectOculus)) / 2.0 + offset.y;
	}
	else
	{
		out.x = remap<float>(aabb.minCorner.x, aabb.maxCorner.x, 0, size.y * aspectOculus, p.x) + (size.x - (size.y * aspectOculus)) / 2.0 + offset.x;
		out.y = remap<float>(aabb.minCorner.z, aabb.maxCorner.z, 0, size.y, p.z) + offset.y;
	}
	return out;
}

int WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow
)
//int main(int, char**)
{
	// Create application window
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("Oculus Monitor"), NULL };
	RegisterClassEx(&wc);
	HWND hwnd = CreateWindow(_T("Oculus Monitor"), _T("Oculus Monitor"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

	// Initialize Direct3D
	if (CreateDeviceD3D(hwnd) < 0)
	{
		CleanupDeviceD3D();
		UnregisterClass(_T("Oculus Monitor"), wc.hInstance);
		return 1;
	}

	// Show the window
	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);

	// Setup Dear ImGui binding
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	// Setup style
	ImGui::StyleColorsDark();

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	bool showSerial = true;
	int currentHudMode = 0;
	bool showSensors = true;
	float pixelDensity = 1.0f;

	ovrGraphicsLuid g_luid;
	ovrInitParams params;
	params.Flags = ovrInit_Invisible;
	params.ConnectionTimeoutMS = 0;
	params.RequestedMinorVersion = 26;
	params.UserData = 0;
	params.LogCallback = 0;

	ovrResult result = ovr_Initialize(&params);
	if (OVR_FAILURE(result))
		return 1;

	result = ovr_Create(&g_HMD, &g_luid);

	currentHudMode = ovr_GetInt(g_HMD, OVR_PERF_HUD_MODE, 0);

	// Main loop
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT)
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}
		if (!IsIconic(hwnd))
		{

			// Start the Dear ImGui frame
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			if (g_HMD)
			{
				auto trackstate = ovr_GetTrackingState(g_HMD, 0, false);

				// Headset
				{
					auto hmddesc = ovr_GetHmdDesc(g_HMD);
					ImGui::Begin("Headset");
					ImGui::Checkbox("Show Serial", &showSerial);
					if (showSerial)
						ImGui::LabelText("Serial", hmddesc.SerialNumber);
					ImGui::LabelText("Manufacturer", hmddesc.Manufacturer);
					ImGui::LabelText("Firmware", "%d.%d", (int)hmddesc.FirmwareMajor, (int)hmddesc.FirmwareMinor);
					ImGui::LabelText("Product", hmddesc.ProductName);
					if ((hmddesc.AvailableHmdCaps&ovrHmdCap_DebugDevice))
						ImGui::Text("Headset is a virtual debug device");
					std::string s;
					if (hmddesc.AvailableTrackingCaps&ovrTrackingCap_Orientation)
						appendstring(s, "Orientation");
					if (hmddesc.AvailableTrackingCaps&ovrTrackingCap_MagYawCorrection)
						appendstring(s, "Mag Yaw Correction");
					if (hmddesc.AvailableTrackingCaps&ovrTrackingCap_Position)
						appendstring(s, "Position");
					ImGui::LabelText("Tracking Cap", s.c_str());
					ImGui::LabelText("Panel Resolution", "%d x %d", hmddesc.Resolution.w, hmddesc.Resolution.h);
					ImGui::LabelText("LIBOVRRT Version", ovr_GetVersionString());
					const char *hudModes[] = { "None", "PerfSummary", "LatencyTiming", "AppRenderTiming", "CompRenderTiming", "VersionInfo", "AswStats" };
					if (ImGui::Combo("Hud Mode", &currentHudMode, hudModes, 7))
					{
						ovr_SetInt(g_HMD, OVR_PERF_HUD_MODE, currentHudMode);
					}
					ImGui::SliderFloat("Pixel Density", &pixelDensity, 0.1, 5.0);
					
					ovrSizei texSize = ovr_GetFovTextureSize(g_HMD, ovrEyeType::ovrEye_Left, hmddesc.DefaultEyeFov[0], pixelDensity);
					ImGui::LabelText("Render Resolution", "%d x %d  (%0.2fMp)", texSize.w, texSize.h, (texSize.w * texSize.h) / 1000000.0f);
					ImGui::End();
				}

				// Controllers
				{
					ImGui::Begin("Controllers");
					unsigned int connected = ovr_GetConnectedControllerTypes(g_HMD);
					bool conn_left = connected & ovrControllerType::ovrControllerType_LTouch;
					bool conn_right = connected & ovrControllerType::ovrControllerType_RTouch;
					bool conn_remote = connected & ovrControllerType::ovrControllerType_Remote;
					bool conn_xbox = connected & ovrControllerType::ovrControllerType_XBox;
					bool conn_object0 = connected & ovrControllerType::ovrControllerType_Object0;
					bool conn_object1 = connected & ovrControllerType::ovrControllerType_Object1;
					bool conn_object2 = connected & ovrControllerType::ovrControllerType_Object2;
					bool conn_object3 = connected & ovrControllerType::ovrControllerType_Object3;

					ImGui::Checkbox("Left connected", &conn_left);
					ImGui::Checkbox("Right connected", &conn_right);
					ImGui::Checkbox("Remote connected", &conn_remote);
					ImGui::Checkbox("XBox connected", &conn_xbox);
					ImGui::Checkbox("VR Object 0 connected", &conn_object0);
					ImGui::Checkbox("VR Object 1 connected", &conn_object1);
					ImGui::Checkbox("VR Object 2 connected", &conn_object2);
					ImGui::Checkbox("VR Object 3 connected", &conn_object3);

					ImGui::End();

					ovrInputState instate;
					if (conn_remote)
					{
						result = ovr_GetInputState(g_HMD, ovrControllerType_Remote, &instate);
						ImGui::Begin("Remote");
						bool remoteLeft = instate.Buttons & ovrButton_Left;
						bool remoteRight = instate.Buttons & ovrButton_Right;
						bool remoteUp = instate.Buttons & ovrButton_Up;
						bool remoteDown = instate.Buttons & ovrButton_Down;
						bool remoteEnter = instate.Buttons & ovrButton_Enter;
						bool remoteBack = instate.Buttons & ovrButton_Back;
						ImGui::Checkbox("Up", &remoteUp);
						ImGui::Checkbox("Down", &remoteDown);
						ImGui::Checkbox("Left", &remoteLeft);
						ImGui::Checkbox("Right", &remoteRight);
						ImGui::Checkbox("Enter", &remoteEnter);
						ImGui::Checkbox("Back", &remoteBack);
						ImGui::End();
					}

					result = ovr_GetInputState(g_HMD, ovrControllerType_Touch, &instate);
					if (conn_left)
					{
						ImGui::Begin("Left Touch");
						ImGui::SliderFloat("Index", &instate.IndexTrigger[0], 0.0f, 1.0f);
						ImGui::SliderFloat("Hand", &instate.HandTrigger[0], 0.0f, 1.0f);
						ImGui::SliderFloat("Thumb X", &instate.ThumbstickNoDeadzone[0].x, -1.0f, 1.0f);
						ImGui::SliderFloat("Thumb Y", &instate.ThumbstickNoDeadzone[0].y, -1.0f, 1.0f);
						bool buttonX = instate.Buttons & ovrButton_X;
						bool buttonY = instate.Buttons & ovrButton_Y;
						bool buttonThumb = instate.Buttons & ovrButton_LThumb;
						bool buttonMenu = instate.Buttons & ovrButton_Enter;
						ImGui::Text("Buttons");
						ImGui::Checkbox("X", &buttonX);
						ImGui::Checkbox("Y", &buttonY);
						ImGui::Checkbox("Thumb", &buttonThumb);
						ImGui::Checkbox("Menu", &buttonMenu);
						bool touchX = instate.Touches & ovrTouch_X;
						bool touchY = instate.Touches & ovrTouch_Y;
						bool touchThumb = instate.Touches & ovrTouch_LThumb;
						bool touchPad = instate.Touches & ovrTouch_LThumbRest;
						bool touchIndex = instate.Touches & ovrTouch_LIndexTrigger;
						bool touchPoint = instate.Touches & ovrTouch_LIndexPointing;
						bool touchThumbUp = instate.Touches & ovrTouch_LThumbUp;
						ImGui::Text("Touches");
						ImGui::Checkbox("X", &touchX);
						ImGui::Checkbox("Y", &touchY);
						ImGui::Checkbox("Thumb", &touchThumb);
						ImGui::Checkbox("Pad", &touchPad);
						ImGui::Checkbox("Index", &touchIndex);
						ImGui::Checkbox("Point", &touchPoint);
						ImGui::Checkbox("Thumbs Up", &touchThumbUp);
						std::string s;
						if (trackstate.HandStatusFlags[0] & ovrStatus_OrientationTracked)
						{
							appendstring(s, "Orientation");
						}
						if (trackstate.HandStatusFlags[0] & ovrStatus_PositionTracked)
						{
							appendstring(s, "Position");
						}
						ImGui::LabelText("Tracking", s.c_str());
						ImGui::LabelText("Pos", "%0.2f, %0.2f, %0.2f", trackstate.HandPoses[0].ThePose.Position.x, trackstate.HandPoses[0].ThePose.Position.y, trackstate.HandPoses[0].ThePose.Position.z);
						ImGui::LabelText("Orientation", "%0.2f, %0.2f, %0.2f, %0.2f", trackstate.HandPoses[0].ThePose.Orientation.w, trackstate.HandPoses[0].ThePose.Orientation.x, trackstate.HandPoses[0].ThePose.Orientation.y, trackstate.HandPoses[0].ThePose.Orientation.z);
						ImGui::LabelText("Vel", "%0.2f, %0.2f, %0.2f", trackstate.HandPoses[0].LinearVelocity.x, trackstate.HandPoses[0].LinearVelocity.y, trackstate.HandPoses[0].LinearVelocity.z);

						ImGui::End();
					}
					if (conn_right)
					{
						ImGui::Begin("Right Touch");
						ImGui::SliderFloat("Index", &instate.IndexTrigger[1], 0.0f, 1.0f);
						ImGui::SliderFloat("Hand", &instate.HandTrigger[1], 0.0f, 1.0f);
						ImGui::SliderFloat("Thumb X", &instate.ThumbstickNoDeadzone[1].x, -1.0f, 1.0f);
						ImGui::SliderFloat("Thumb Y", &instate.ThumbstickNoDeadzone[1].y, -1.0f, 1.0f);
						bool buttonA = instate.Buttons & ovrButton_A;
						bool buttonB = instate.Buttons & ovrButton_B;
						bool buttonThumb = instate.Buttons & ovrButton_RThumb;
						ImGui::Text("Buttons");
						ImGui::Checkbox("A", &buttonA);
						ImGui::Checkbox("B", &buttonB);
						ImGui::Checkbox("Thumb", &buttonThumb);
						bool touchA = instate.Touches & ovrTouch_A;
						bool touchB = instate.Touches & ovrTouch_B;
						bool touchThumb = instate.Touches & ovrTouch_RThumb;
						bool touchPad = instate.Touches & ovrTouch_RThumbRest;
						bool touchIndex = instate.Touches & ovrTouch_RIndexTrigger;
						bool touchPoint = instate.Touches & ovrTouch_RIndexPointing;
						bool touchThumbUp = instate.Touches & ovrTouch_RThumbUp;
						ImGui::Text("Touches");
						ImGui::Checkbox("A", &touchA);
						ImGui::Checkbox("B", &touchB);
						ImGui::Checkbox("Thumb", &touchThumb);
						ImGui::Checkbox("Pad", &touchPad);
						ImGui::Checkbox("Index", &touchIndex);
						ImGui::Checkbox("Point", &touchPoint);
						ImGui::Checkbox("Thumbs Up", &touchThumbUp);
						std::string s;
						if (trackstate.HandStatusFlags[1] & ovrStatus_OrientationTracked)
						{
							appendstring(s, "Orientation");
						}
						if (trackstate.HandStatusFlags[1] & ovrStatus_PositionTracked)
						{
							appendstring(s, "Position");
						}
						ImGui::LabelText("Tracking", s.c_str());
						ImGui::LabelText("Pos", "%0.2f, %0.2f, %0.2f", trackstate.HandPoses[1].ThePose.Position.x, trackstate.HandPoses[1].ThePose.Position.y, trackstate.HandPoses[1].ThePose.Position.z);
						ImGui::LabelText("Orientation", "%0.2f, %0.2f, %0.2f, %0.2f", trackstate.HandPoses[1].ThePose.Orientation.w, trackstate.HandPoses[1].ThePose.Orientation.x, trackstate.HandPoses[1].ThePose.Orientation.y, trackstate.HandPoses[1].ThePose.Orientation.z);
						ImGui::LabelText("Vel", "%0.2f, %0.2f, %0.2f", trackstate.HandPoses[1].LinearVelocity.x, trackstate.HandPoses[1].LinearVelocity.y, trackstate.HandPoses[1].LinearVelocity.z);
						ImGui::End();
					}
				}
				{
					ImGui::Begin("Room Layout");
					ImGui::Checkbox("Show Sensors", &showSensors);

					AABB aabb;
					ImVec2 size = ImGui::GetContentRegionMax();
					ImVec2 offset = ImGui::GetWindowPos();
					int outerCount = 0;
					int playCount = 0;
					ovr_GetBoundaryGeometry(g_HMD, ovrBoundaryType::ovrBoundary_Outer, 0, &outerCount);
					ovr_GetBoundaryGeometry(g_HMD, ovrBoundaryType::ovrBoundary_PlayArea, 0, &playCount);

					std::vector<ovrVector3f> outerPoints3d(outerCount);
					std::vector<ovrVector3f> playPoints3d(playCount);

					if (outerCount > 0)
					{
						ovr_GetBoundaryGeometry(g_HMD, ovrBoundaryType::ovrBoundary_Outer, &(outerPoints3d[0]), &outerCount);
						for (int i = 0; i < outerCount; ++i)
						{
							aabb.merge(outerPoints3d[i]);
						}
					}

					if (playCount > 0)
					{
						ovr_GetBoundaryGeometry(g_HMD, ovrBoundaryType::ovrBoundary_PlayArea, &(playPoints3d[0]), &playCount);
						for (int i = 0; i < playCount; ++i)
						{
							aabb.merge(playPoints3d[i]);
						}
					}

					int sensorCount = ovr_GetTrackerCount(g_HMD);
					if (showSensors)
					{
						for (int i = 0; i < sensorCount; ++i)
						{
							aabb.merge(ovr_GetTrackerPose(g_HMD, i).LeveledPose.Position);
						}
					}

					//aabb.merge(trackstate.HeadPose.ThePose.Position);
					//aabb.merge(trackstate.HandPoses[0].ThePose.Position);
					//aabb.merge(trackstate.HandPoses[1].ThePose.Position);
					aabb.minCorner -= OVR::Vector3f(0.2, 0.2, 0.2);
					aabb.maxCorner += OVR::Vector3f(0.2, 0.2, 0.2);

					std::vector<ImVec2> outerPoints2d(outerCount);
					std::vector<ImVec2> playPoints2d(playCount);
					for (int i = 0; i < outerCount; ++i)
					{
						outerPoints2d[i] = remap(aabb, size, offset, outerPoints3d[i]);
					}
					if (outerCount > 0)
					{
						ImGui::GetWindowDrawList()->AddPolyline(&outerPoints2d[0], outerCount, 0xffffffff, true, 1);
					}

					for (int i = 0; i < playCount; ++i)
					{
						playPoints2d[i] = remap(aabb, size, offset, playPoints3d[i]);
					}
					if (outerCount > 0)
					{
						ImGui::GetWindowDrawList()->AddPolyline(&playPoints2d[0], playCount, 0xffffff00, true, 1);
					}

					ImVec2 pos;
					ImVec2 pos1;
					ImVec2 pos2;
					ImVec2 pos3;
					ImVec2 pos4;

					pos = remap(aabb, size, offset, trackstate.HandPoses[0].ThePose.Position);
					ImGui::GetWindowDrawList()->AddCircle(pos, 10, 0xffffffff);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.HandPoses[0].ThePose.Position) + OVR::Quatf(trackstate.HandPoses[0].ThePose.Orientation) * OVR::Vector3f(0.2, 0, 0));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xff0000ff);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.HandPoses[0].ThePose.Position) + OVR::Quatf(trackstate.HandPoses[0].ThePose.Orientation) * OVR::Vector3f(0, 0.2, 0));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xff00ff00);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.HandPoses[0].ThePose.Position) + OVR::Quatf(trackstate.HandPoses[0].ThePose.Orientation) * OVR::Vector3f(0, 0, -0.2));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xffff3030);

					pos = remap(aabb, size, offset, trackstate.HandPoses[1].ThePose.Position);
					ImGui::GetWindowDrawList()->AddCircle(pos, 10, 0xffffffff);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.HandPoses[1].ThePose.Position) + OVR::Quatf(trackstate.HandPoses[1].ThePose.Orientation) * OVR::Vector3f(0.2, 0, 0));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xff0000ff);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.HandPoses[1].ThePose.Position) + OVR::Quatf(trackstate.HandPoses[1].ThePose.Orientation) * OVR::Vector3f(0, 0.2, 0));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xff00ff00);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.HandPoses[1].ThePose.Position) + OVR::Quatf(trackstate.HandPoses[1].ThePose.Orientation) * OVR::Vector3f(0, 0, -0.2));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xffff3030);

					pos = remap(aabb, size, offset, trackstate.HeadPose.ThePose.Position);
					ImGui::GetWindowDrawList()->AddCircle(pos, 10, 0xff00ffff);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.HeadPose.ThePose.Position) + OVR::Quatf(trackstate.HeadPose.ThePose.Orientation) * OVR::Vector3f(0.2, 0, 0));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xff0000ff);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.HeadPose.ThePose.Position) + OVR::Quatf(trackstate.HeadPose.ThePose.Orientation) * OVR::Vector3f(0, 0.2, 0));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xff00ff00);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.HeadPose.ThePose.Position) + OVR::Quatf(trackstate.HeadPose.ThePose.Orientation) * OVR::Vector3f(0, 0, -0.2));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xffff3030);

					pos = remap(aabb, size, offset, trackstate.CalibratedOrigin.Position);
					ImGui::GetWindowDrawList()->AddCircle(pos, 10, 0xff0000ff);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.CalibratedOrigin.Position) + OVR::Quatf(trackstate.CalibratedOrigin.Orientation) * OVR::Vector3f(0.2, 0, 0));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xff0000ff);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.CalibratedOrigin.Position) + OVR::Quatf(trackstate.CalibratedOrigin.Orientation) * OVR::Vector3f(0, 0.2, 0));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xff00ff00);
					pos2 = remap(aabb, size, offset, OVR::Vector3f(trackstate.CalibratedOrigin.Position) + OVR::Quatf(trackstate.CalibratedOrigin.Orientation) * OVR::Vector3f(0, 0, -0.2));
					ImGui::GetWindowDrawList()->AddLine(pos, pos2, 0xffff3030);

					if (showSensors)
					{
						for (int i = 0; i < sensorCount; ++i)
						{
							ovrTrackerDesc desc = ovr_GetTrackerDesc(g_HMD, i);
							ovrTrackerPose pose = ovr_GetTrackerPose(g_HMD, i);
							float fov = desc.FrustumHFovInRadians / 2.0;
							pos = remap(aabb, size, offset, pose.Pose.Position);
							ImGui::GetWindowDrawList()->AddCircle(pos, 10, 0xffff00ff);
							OVR::Quatf q = pose.LeveledPose.Orientation;
							OVR::Vector3f dir = q * OVR::Vector3f(0, 0, 1);
							OVR::Vector3f dirPerp(-dir.z, dir.y, dir.x);
							float x = sin(fov);
							float y = cos(fov);
							float scaleNear = desc.FrustumNearZInMeters / y;
							float scaleFar = desc.FrustumFarZInMeters / y;

							pos1 = remap(aabb, size, offset, OVR::Vector3f(pose.Pose.Position) + dir * y*scaleNear + dirPerp * -x * scaleNear);
							pos2 = remap(aabb, size, offset, OVR::Vector3f(pose.Pose.Position) + dir * y*scaleNear + dirPerp * x*scaleNear);
							pos3 = remap(aabb, size, offset, OVR::Vector3f(pose.Pose.Position) + dir * y*scaleFar + dirPerp * -x * scaleFar);
							pos4 = remap(aabb, size, offset, OVR::Vector3f(pose.Pose.Position) + dir * y*scaleFar + dirPerp * x*scaleFar);

							ImGui::GetWindowDrawList()->AddQuadFilled(pos1, pos3, pos4, pos2, 0x20ff00ff);
							ImGui::GetWindowDrawList()->AddLine(pos, pos3, 0xffff00ff);
							ImGui::GetWindowDrawList()->AddLine(pos, pos4, 0xffff00ff);
						}
					}
					ImGui::End();
				}
			}
			else
			{
				ImGui::Begin("Error");
				ImGui::Text("Failed to create the session");
				ImGui::End();
			}

			// Rendering
			ImGui::Render();
			g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
			g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

			g_pSwapChain->Present(1, 0); // Present with vsync
										 //g_pSwapChain->Present(0, 0); // Present without vsync
		}
		else
		{
			Sleep(100);
		}
	}

	ovr_Destroy(g_HMD);

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	DestroyWindow(hwnd);
	UnregisterClass(_T("Oculus Monitor"), wc.hInstance);

	return 0;
}
