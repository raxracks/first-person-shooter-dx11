//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "RenderTexture.h"

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

    std::unique_ptr<DirectX::Model> m_weapon;
    
    //std::unique_ptr<DirectX::GeometricPrimitive> m_weapon;
    std::unique_ptr<DirectX::GeometricPrimitive> m_room;

    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_proj;
    DirectX::SimpleMath::Matrix m_gunProj;

    float m_pitch;
    float m_yaw;
    DirectX::SimpleMath::Vector3 m_cameraPos;

    DirectX::SimpleMath::Color m_roomColor;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_roomTex;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_crosshair;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_crosshair_h;

    DirectX::GamePad::ButtonStateTracker m_buttons;
    DirectX::Keyboard::KeyboardStateTracker m_keys;
    DirectX::Mouse::ButtonStateTracker m_mouseButtons;

    bool m_aiming;

    float m_fov;
    float m_fov_previous = m_fov;

    float m_hipfire_fov;
    float m_aiming_fov;

    bool m_using_keyboard;

    float m_near;
    float m_far;

    bool m_sprinting;

    std::unique_ptr<DX::RenderTexture> m_renderTexture;
    std::unique_ptr<DirectX::SpriteBatch> m_sprites;

    DirectX::SimpleMath::Vector3 m_weaponOffset;
    DirectX::SimpleMath::Vector3 m_weaponRotation;

    std::unique_ptr<DirectX::IEffectFactory> m_fxFactory;
    std::unique_ptr<DirectX::CommonStates> m_states;

    DirectX::SimpleMath::Vector2 m_screenPos;
    DirectX::SimpleMath::Vector2 m_origin;
    DirectX::SimpleMath::Vector2 m_origin_h;

    float m_crosshair_spread;

    float m_steps;

    bool m_walking = false;
};
