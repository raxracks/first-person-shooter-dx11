//
// Game.cpp
//

#include "pch.h"
#include "Game.h"
#include <Helpers.h>
#include <SpriteBatch.h>

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
	// Dumb hardcoded shit
	const XMVECTORF32 START_POSITION			= { 0.f, -1.5f, 0.f, 0.f };
	const XMVECTORF32 ROOM_BOUNDS				= { 8.f, 6.f, 12.f, 0.f };

	// Controller
	const float ROTATION_GAIN					= 3.8f;
	const float AIMING_ROTATION_GAIN			= 1.8f;

	// Mouse + Keyboard
	const float MOUSE_ROTATION_GAIN				= 0.35f;
	const float MOUSE_AIMING_ROTATION_GAIN		= 0.15f;

	// Both
	const float MOVEMENT_GAIN					= 3.7f;
	const float MOVEMENT_SPRINTING_GAIN			= 10.7f;

	// Weapon
	constexpr Vector3 WEAPON_POSITION			= { 3.0f, -4.0f, -6.0f };
	constexpr Vector3 WEAPON_POSITION_AIMING	= { 0.0f, -1.25f, -6.0f };
}

Game::Game() noexcept(false) :
	m_pitch(0),
	m_yaw(0),
	m_cameraPos(START_POSITION),
	m_roomColor(Colors::White),
	m_weaponOffset(WEAPON_POSITION)
{
	m_deviceResources = std::make_unique<DX::DeviceResources>();
	// TODO: Provide parameters for swapchain format, depth/stencil format, and backbuffer count.
	//   Add DX::DeviceResources::c_AllowTearing to opt-in to variable rate displays.
	//   Add DX::DeviceResources::c_EnableHDR for HDR10 display.
	m_deviceResources->RegisterDeviceNotify(this);

	m_renderTexture = std::make_unique<DX::RenderTexture>(
		m_deviceResources->GetBackBufferFormat());
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
			else {
				if(!m_using_keyboard) m_sprinting = false;
			}
			if (pad.IsLeftThumbStickDown()) {
				move.z = pad.thumbSticks.leftY;
				m_using_keyboard = false;
			}

			if (pad.IsLeftStickPressed()) {
				m_sprinting = true;
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
	else {
		if(m_using_keyboard) m_sprinting = false;
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

	if (kb.LeftShift) {
		m_sprinting = true;
	}

	//---------------------------------------------
	// Agnostic control
	// --------------------------------------------

	// Change FOV based on aiming state
	m_fov = (m_aiming ?
		Helpers::Lerp(m_hipfire_fov, m_aiming_fov, m_fov, elapsedTime * 400) :
		Helpers::Lerp(m_aiming_fov, m_hipfire_fov, m_fov, elapsedTime * 400));

	m_weaponOffset = (m_aiming ?
		Helpers::LerpVector3(WEAPON_POSITION, WEAPON_POSITION_AIMING, m_weaponOffset, elapsedTime * 10) :
		Helpers::LerpVector3(WEAPON_POSITION_AIMING, WEAPON_POSITION, m_weaponOffset, elapsedTime * 10));

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
	move = Vector3::Transform(move, q) * (m_sprinting ? MOVEMENT_SPRINTING_GAIN : MOVEMENT_GAIN) * elapsedTime;

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

	m_weapon->Draw(Matrix::Identity, Matrix::CreateTranslation(m_weaponOffset), m_proj);

	auto context = m_deviceResources->GetD3DDeviceContext();

	auto renderTarget = m_deviceResources->GetRenderTargetView();
	context->OMSetRenderTargets(1, &renderTarget, nullptr);

	m_room->Draw(Matrix::Identity, m_view, m_proj,
		m_roomColor, m_roomTex.Get());

	m_sprites->Begin();

	m_sprites->Draw(m_renderTexture->GetShaderResourceView(),
		m_deviceResources->GetOutputSize());

	m_sprites->End();

	ID3D11ShaderResourceView* nullsrv[] = { nullptr };
	context->PSSetShaderResources(0, 1, nullsrv);

	// Show the new frame.
	m_deviceResources->Present();

	//auto context = m_deviceResources->GetD3DDeviceContext();
	//PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

	//m_room->Draw(Matrix::Identity, m_view, m_proj,
	//	m_roomColor, m_roomTex.Get());

	//auto renderTarget = m_deviceResources->GetRenderTargetView();
	//auto depthStencil = m_deviceResources->GetDepthStencilView();

	//context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
	//context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	//context->OMSetRenderTargets(1, &renderTarget, depthStencil);

	//// TODO: Add your rendering code here.
	//m_room->Draw(Matrix::Identity, m_view, m_proj,
	//	m_roomColor, m_roomTex.Get());

	//m_sprites->Begin();

	//m_sprites->Draw(m_renderTexture->GetShaderResourceView(),
	//	m_deviceResources->GetOutputSize());

	//m_sprites->End();

	//PIXEndEvent(context);

	//// Show the new frame.
	//PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
	//m_deviceResources->Present();
	//PIXEndEvent();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
	auto context = m_deviceResources->GetD3DDeviceContext();
	PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

	auto renderTarget = m_renderTexture->GetRenderTargetView();
	auto depthStencil = m_deviceResources->GetDepthStencilView();

	context->ClearRenderTargetView(renderTarget, Colors::Transparent);
	context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	context->OMSetRenderTargets(1, &renderTarget, depthStencil);

	// Clear the views.
	

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
	m_weapon = GeometricPrimitive::CreateBox(context,
		XMFLOAT3(2.0f, 2.0f, 5.0f));

	DX::ThrowIfFailed(
		CreateDDSTextureFromFile(device, L"Assets/roomtexture.dds",
			nullptr, m_roomTex.ReleaseAndGetAddressOf()));

	m_sprites = std::make_unique<SpriteBatch>(context);

	m_renderTexture->SetDevice(device);

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

	m_renderTexture->SetWindow(size);

	m_sprites->SetRotation(m_deviceResources->GetRotation());
}

void Game::OnDeviceLost()
{
	// TODO: Add Direct3D resource cleanup here.
	m_room.reset();
	m_roomTex.Reset();
	m_mouse->SetMode(Mouse::MODE_ABSOLUTE);
	m_sprites.reset();
	m_renderTexture->ReleaseDevice();
}

void Game::OnDeviceRestored()
{
	CreateDeviceDependentResources();

	CreateWindowSizeDependentResources();
}
#pragma endregion
