//
// Game.cpp
//

#include "pch.h"
#include "Game.h"
#include <Helpers.h>

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
	const XMVECTORF32 START_POSITION = { 0.f, -1.5f, 0.f, 0.f };
	const XMVECTORF32 ROOM_BOUNDS = { 8.f, 6.f, 12.f, 0.f };

	constexpr float ROTATION_GAIN = 3.2f;
	constexpr float AIMING_ROTATION_GAIN = 1.8f;

	constexpr float MOUSE_ROTATION_GAIN = 0.3f;
	constexpr float MOUSE_AIMING_ROTATION_GAIN = 0.1f;
	constexpr float MOVEMENT_GAIN = 3.7f;
}

Game::Game() noexcept(false) :
	m_pitch(0),
	m_yaw(0),
	m_cameraPos(START_POSITION),
	m_roomColor(Colors::White)
{
	m_deviceResources = std::make_unique<DX::DeviceResources>();
	// TODO: Provide parameters for swapchain format, depth/stencil format, and backbuffer count.
	//   Add DX::DeviceResources::c_AllowTearing to opt-in to variable rate displays.
	//   Add DX::DeviceResources::c_EnableHDR for HDR10 display.
	m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(::IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
	m_deviceResources->SetWindow(window, width, height, rotation);

	m_deviceResources->CreateDeviceResources();
	CreateDeviceDependentResources();

	m_deviceResources->CreateWindowSizeDependentResources();
	CreateWindowSizeDependentResources();

	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/

	m_gamePad = std::make_unique<GamePad>();
	m_keyboard = std::make_unique<Keyboard>();
	m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

	m_mouse = std::make_unique<Mouse>();
	m_mouse->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
}

#pragma region Frame Update
void Game::Tick()
{
	m_timer.Tick([&]()
		{
			Update(m_timer);
		});

	Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
	float elapsedTime = float(timer.GetElapsedSeconds());

	PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

	//---------------------------------------------
	// Update FOV on change
	// --------------------------------------------
	if (m_fov != m_fov_previous) {
		auto size = m_deviceResources->GetOutputSize();
		m_proj = Matrix::CreatePerspectiveFieldOfView(
			XMConvertToRadians(m_fov),
			float(size.right) / float(size.bottom), m_near, m_far);

		m_fov_previous = m_fov;
	}

	// Movement vector
	Vector3 move = Vector3::Zero;

	//---------------------------------------------
	// Gamepad
	// --------------------------------------------
	auto pad = m_gamePad->GetState(0);
	if (pad.IsConnected())
	{
		m_buttons.Update(pad);

		if (pad.IsMenuPressed())
		{
			// TODO: Replace with pause menu
			ExitGame();
		}

		if (pad.triggers.left > 0.2f) {
			m_aiming = true;
			m_using_keyboard = false;
		}
		else {
			m_aiming = false;
		}

		// Movement
		{
			if (pad.IsLeftThumbStickLeft()) {
				move.x = -pad.thumbSticks.leftX;
				m_using_keyboard = false;
			}
			if (pad.IsLeftThumbStickRight()) {
				move.x = -pad.thumbSticks.leftX;
				m_using_keyboard = false;
			}
			if (pad.IsLeftThumbStickUp()) {
				move.z = pad.thumbSticks.leftY;
				m_using_keyboard = false;
			}
			if (pad.IsLeftThumbStickDown()) {
				move.z = pad.thumbSticks.leftY;
				m_using_keyboard = false;
			}
		}

		// Rotation
		{
			if (pad.IsRightThumbStickDown() || pad.IsRightThumbStickUp() || pad.IsRightThumbStickLeft() || pad.IsRightThumbStickRight()) m_using_keyboard = false;
			m_yaw += -pad.thumbSticks.rightX * ROTATION_GAIN * elapsedTime;
			m_pitch += pad.thumbSticks.rightY * ROTATION_GAIN * elapsedTime;
		}
	}
	else {
		m_buttons.Reset();
	}

	//---------------------------------------------
	// Mouse
	// --------------------------------------------
	auto mouse = m_mouse->GetState();
	if (m_using_keyboard) {
		m_mouseButtons.Update(mouse);
		m_aiming = mouse.rightButton;
	}

	if (mouse.leftButton || mouse.middleButton || mouse.rightButton)  m_using_keyboard = true;

	if (mouse.positionMode == Mouse::MODE_RELATIVE)
	{
		Vector3 delta = Vector3(float(mouse.x), float(mouse.y), 0.f)
			* (m_aiming ? MOUSE_AIMING_ROTATION_GAIN : MOUSE_ROTATION_GAIN) * elapsedTime;

		m_pitch -= delta.y;
		m_yaw -= delta.x;
	}

	// TODO: Replace with actual logic
	m_mouse->SetMode(Mouse::MODE_RELATIVE);

	//---------------------------------------------
	// Keyboard
	// --------------------------------------------
	auto kb = m_keyboard->GetState();
	if (kb.Escape)
	{
		// TODO: Replace with pause menu
		ExitGame();
		m_using_keyboard = true;
	}

	m_keys.Update(kb);

	if (kb.Up || kb.W) {
		move.z = 1.0f;
		m_using_keyboard = true;
	}

	if (kb.Down || kb.S) {
		move.z = -1.0f;
		m_using_keyboard = true;
	}

	if (kb.Left || kb.A) {
		move.x = 1.0f;
		m_using_keyboard = true;
	}

	if (kb.Right || kb.D) {
		move.x = -1.0f;
		m_using_keyboard = true;
	}

	//---------------------------------------------
	// Agnostic control
	// --------------------------------------------

	// Change FOV based on aiming state
	m_fov = (m_aiming ?
		Helpers::Lerp(m_hipfire_fov, m_aiming_fov, m_fov, elapsedTime * 400) :
		Helpers::Lerp(m_aiming_fov, m_hipfire_fov, m_fov, elapsedTime * 400));

	// Limit camera rotation
	constexpr float limit = XM_PIDIV2 - 0.01f;
	m_pitch = std::max(-limit, m_pitch);
	m_pitch = std::min(+limit, m_pitch);

	if (m_yaw > XM_PI)
	{
		m_yaw -= XM_2PI;
	}
	else if (m_yaw < -XM_PI)
	{
		m_yaw += XM_2PI;
	}

	Quaternion q = Quaternion::CreateFromYawPitchRoll(m_yaw, 0.0f, 0.0f);
	move = Vector3::Transform(move, q) * MOVEMENT_GAIN * elapsedTime;

	// Move camera by movement vector
	m_cameraPos += move;

	// Check if camera is in room bounds
	Vector3 halfBound = (Vector3(ROOM_BOUNDS.v) / Vector3(2.f))
		- Vector3(0.1f, 0.1f, 0.1f);

	m_cameraPos = Vector3::Min(m_cameraPos, halfBound);
	m_cameraPos = Vector3::Max(m_cameraPos, -halfBound);

	// Calculate positions for lookAt vector
	float y = sinf(m_pitch);
	float r = cosf(m_pitch);
	float z = r * cosf(m_yaw);
	float x = r * sinf(m_yaw);

	XMVECTOR lookAt = m_cameraPos + Vector3(x, y, z);

	m_view = XMMatrixLookAtRH(m_cameraPos, lookAt, Vector3::Up);

	// TODO: Remove
	if (m_buttons.a == GamePad::ButtonStateTracker::PRESSED || m_keys.pressed.Tab)
	{
		if (m_roomColor == Colors::Red.v)
			m_roomColor = Colors::Green;
		else if (m_roomColor == Colors::Green.v)
			m_roomColor = Colors::Blue;
		else if (m_roomColor == Colors::Blue.v)
			m_roomColor = Colors::White;
		else
			m_roomColor = Colors::Red;
	}

	PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
	// Don't try to render anything before the first Update.
	if (m_timer.GetFrameCount() == 0)
	{
		return;
	}

	Clear();

	auto context = m_deviceResources->GetD3DDeviceContext();
	PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

	// TODO: Add your rendering code here.
	m_room->Draw(Matrix::Identity, m_view, m_proj,
		m_roomColor, m_roomTex.Get());

	PIXEndEvent(context);

	// Show the new frame.
	PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
	m_deviceResources->Present();
	PIXEndEvent();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
	auto context = m_deviceResources->GetD3DDeviceContext();
	PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

	// Clear the views.
	auto renderTarget = m_deviceResources->GetRenderTargetView();
	auto depthStencil = m_deviceResources->GetDepthStencilView();

	context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
	context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	context->OMSetRenderTargets(1, &renderTarget, depthStencil);

	// Set the viewport.
	auto const viewport = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

	PIXEndEvent(context);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
	// TODO: Game is becoming active window.
	m_gamePad->Resume();
	m_buttons.Reset();
	m_keys.Reset();
	m_mouseButtons.Reset();
}

void Game::OnDeactivated()
{
	// TODO: Game is becoming background window.
	m_gamePad->Suspend();
}

void Game::OnSuspending()
{
	auto context = m_deviceResources->GetD3DDeviceContext();
	context->ClearState();

	m_deviceResources->Trim();

	// TODO: Game is being power-suspended.
	m_gamePad->Suspend();
}

void Game::OnResuming()
{
	m_timer.ResetElapsedTime();

	// TODO: Game is being power-resumed.
	m_gamePad->Resume();
	m_buttons.Reset();
	m_keys.Reset();
	m_mouseButtons.Reset();
}

void Game::OnDisplayChange()
{
	m_deviceResources->UpdateColorSpace();
}

void Game::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
	if (!m_deviceResources->WindowSizeChanged(width, height, rotation))
		return;

	CreateWindowSizeDependentResources();

	// TODO: Game window is being resized.
}

void Game::ValidateDevice()
{
	m_deviceResources->ValidateDevice();
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const noexcept
{
	// TODO: Change to desired default window size (note minimum size is 320x200).
	width = 800;
	height = 600;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
	auto device = m_deviceResources->GetD3DDevice();

	// TODO: Initialize device dependent objects here (independent of window size).
	auto context = m_deviceResources->GetD3DDeviceContext();
	m_room = GeometricPrimitive::CreateBox(context,
		XMFLOAT3(ROOM_BOUNDS[0], ROOM_BOUNDS[1], ROOM_BOUNDS[2]),
		false, true);

	DX::ThrowIfFailed(
		CreateDDSTextureFromFile(device, L"Assets/roomtexture.dds",
			nullptr, m_roomTex.ReleaseAndGetAddressOf()));

	device;
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
	// TODO: Initialize windows-size dependent objects here.
	auto size = m_deviceResources->GetOutputSize();
	m_proj = Matrix::CreatePerspectiveFieldOfView(
		XMConvertToRadians(m_fov),
		float(size.right) / float(size.bottom), m_near, m_far);
}

void Game::OnDeviceLost()
{
	// TODO: Add Direct3D resource cleanup here.
	m_room.reset();
	m_roomTex.Reset();
	m_mouse->SetMode(Mouse::MODE_ABSOLUTE);
}

void Game::OnDeviceRestored()
{
	CreateDeviceDependentResources();

	CreateWindowSizeDependentResources();
}
#pragma endregion
