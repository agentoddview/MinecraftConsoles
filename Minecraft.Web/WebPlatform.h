#pragma once

#include <string_view>

namespace web {

class IRuntimeClock {
public:
    virtual ~IRuntimeClock() = default;
    virtual double NowMs() const = 0;
};

class IWindowLifecycle {
public:
    virtual ~IWindowLifecycle() = default;
    virtual void SetStatus(std::string_view text) = 0;
    virtual void PublishUiState(std::string_view json) = 0;
    virtual void RequestPointerLock() = 0;
    virtual void ReleasePointerLock() = 0;
    virtual void RequestFullscreen() = 0;
};

class IAudioService {
public:
    virtual ~IAudioService() = default;
    virtual void Tick(double deltaSeconds) = 0;
};

class IPlatformServices {
public:
    virtual ~IPlatformServices() = default;
    virtual bool SupportsMultiplayer() const = 0;
    virtual bool SupportsExistingSaveImport() const = 0;
};

class BrowserPlatform final : public IRuntimeClock,
                              public IWindowLifecycle,
                              public IAudioService,
                              public IPlatformServices {
public:
    double NowMs() const override;
    void SetStatus(std::string_view text) override;
    void PublishUiState(std::string_view json) override;
    void RequestPointerLock() override;
    void ReleasePointerLock() override;
    void RequestFullscreen() override;
    void Tick(double deltaSeconds) override;
    bool SupportsMultiplayer() const override;
    bool SupportsExistingSaveImport() const override;

    void InitializePersistentStorage() const;
    void FlushPersistentStorage() const;
    bool IsPointerLocked() const;
};

} // namespace web
