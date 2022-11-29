//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"


// A basic game implementation that creates a D3D11 device and
// provides a game loop.
class Game final : public DX::IDeviceNotify
{
public:
    Game() noexcept(false);
    ~Game() = default;

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    // Initialization and management
    void Initialize(::IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnDisplayChange();
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
    void ValidateDevice();

    // Properties
    void GetDefaultSize( int& width, int& height ) const noexcept;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    std::unique_ptr<DirectX::GamePad> m_gamePad;
    std::unique_ptr<DirectX::Keyboard> m_keyboard;
    std::unique_ptr<DirectX::Mouse> m_mouse;

    std::unique_ptr<DirectX::GeometricPrimitive> m_room;

    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_proj;

    float m_pitch;
    float m_yaw;
    DirectX::SimpleMath::Vector3 m_cameraPos;

    DirectX::SimpleMath::Color m_roomColor;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_roomTex;

    DirectX::GamePad::ButtonStateTracker m_buttons;
    DirectX::Keyboard::KeyboardStateTracker m_keys;
    DirectX::Mouse::ButtonStateTracker m_mouseButtons;

    bool m_aiming = false;

    float m_fov = 90.0f;
    float m_fov_previous = m_fov;

    float m_hipfire_fov = 100.0f;
    float m_aiming_fov = 70.0f;

    bool m_using_keyboard = false;

    float m_near = 0.01f;
    float m_far = 5000.0f;
};
