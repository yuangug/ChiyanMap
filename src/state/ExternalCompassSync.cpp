#include "state/ExternalCompassSync.h"

#include "state/MapRenderState.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>

extern float g_playerYaw;
extern float g_playerX;
extern float g_playerZ;

namespace ExternalCompassSync {
    namespace {
        using namespace std::chrono_literals;
        namespace bt = winrt::Windows::Devices::Bluetooth;
        namespace advertisement = winrt::Windows::Devices::Bluetooth::Advertisement;
        namespace gatt = winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
        namespace enumeration = winrt::Windows::Devices::Enumeration;
        namespace streams = winrt::Windows::Storage::Streams;

        constexpr winrt::guid AdvancedServiceUuid{0x0000fa00, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}};
        constexpr winrt::guid VirtualAzimuthUuid{0x0000fa02, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}};
        constexpr auto KeepAliveInterval = 500ms;
        constexpr auto StandardRetryInterval = 5s;
        constexpr auto AdapterRetryInterval = 15s;
        constexpr uint32_t DeviceNotReadyHResult = 0x800710df;

        enum class ScanResult {
            Found,
            NotFound,
            AdapterUnavailable,
            Failed,
        };

        enum class ResolveResult {
            Connected,
            RetryScan,
            RetryConnect,
            AdapterUnavailable,
        };

        std::atomic<bool> g_running{false};
        std::atomic<bool> g_wakeRequested{false};
        std::thread g_worker;
        std::mutex g_workerMutex;

        std::mutex g_targetMutex;
        bool g_targetEnabled = false;
        std::string g_targetId;
        float g_targetX = 0.0f;
        float g_targetZ = 0.0f;
        int g_targetDimensionId = 0;

        void LogFailure(const char* where, winrt::hresult_error const& e) {
            std::ofstream out("MapMod_Debug.txt", std::ios::app);
            if (out.is_open()) {
                out << "[ExternalCompassSync] " << where << " failed: 0x"
                    << std::hex << static_cast<uint32_t>(e.code()) << "\n";
            }
        }

        void LogFailure(const char* where) {
            std::ofstream out("MapMod_Debug.txt", std::ios::app);
            if (out.is_open()) {
                out << "[ExternalCompassSync] " << where << " failed\n";
            }
        }

        void LogMessage(const std::string& message) {
            std::ofstream out("MapMod_Debug.txt", std::ios::app);
            if (out.is_open()) {
                out << "[ExternalCompassSync] " << message << "\n";
            }
        }

        void ResetConnection(
            bt::BluetoothLEDevice& device,
            gatt::GattDeviceService& service,
            gatt::GattCharacteristic& characteristic
        ) {
            characteristic = nullptr;
            if (service) {
                try {
                    service.Close();
                } catch (winrt::hresult_error const& e) {
                    LogFailure("GattDeviceService::Close", e);
                } catch (...) {
                    LogFailure("GattDeviceService::Close");
                }
                service = nullptr;
            }
            device = nullptr;
        }

        std::string HexAddress(uint64_t address) {
            std::ostringstream oss;
            oss << std::hex << std::uppercase << std::setfill('0') << std::setw(12) << address;
            return oss.str();
        }

        float NormalizeDegrees(float angle) {
            while (angle < 0.0f) angle += 360.0f;
            while (angle >= 360.0f) angle -= 360.0f;
            return angle;
        }

        float AngleDistance(float a, float b) {
            float diff = std::fabs(NormalizeDegrees(a) - NormalizeDegrees(b));
            return diff > 180.0f ? 360.0f - diff : diff;
        }

        bool ResolveOutputAngle(float& angle) {
            bool targetEnabled = false;
            float targetX = 0.0f;
            float targetZ = 0.0f;
            int targetDimensionId = 0;
            {
                std::lock_guard<std::mutex> lock(g_targetMutex);
                targetEnabled = g_targetEnabled;
                targetX = g_targetX;
                targetZ = g_targetZ;
                targetDimensionId = g_targetDimensionId;
            }

            if (targetEnabled && targetDimensionId == MapRenderState::currentDimensionId) {
                float dx = targetX - g_playerX;
                float dz = targetZ - g_playerZ;
                if (std::fabs(dx) < 0.001f && std::fabs(dz) < 0.001f) {
                    angle = 0.0f;
                } else {
                    float targetBearing = NormalizeDegrees(std::atan2(-dx, dz) * 180.0f / 3.14159265358979323846f);
                    angle = NormalizeDegrees(targetBearing - NormalizeDegrees(g_playerYaw));
                }
                return true;
            }

            if (MapRenderState::currentDimensionId == 1) {
                return false;
            }

            angle = NormalizeDegrees(g_playerYaw);
            return true;
        }

        std::wstring WidenAscii(const std::string& value) {
            return std::wstring(value.begin(), value.end());
        }

        bool AdvertisementHasService(advertisement::BluetoothLEAdvertisement const& adv) {
            for (auto const& uuid : adv.ServiceUuids()) {
                if (uuid == AdvancedServiceUuid) {
                    return true;
                }
            }
            return false;
        }

        ScanResult FindDeviceAddress(const std::string& deviceName, uint64_t& address, bt::BluetoothAddressType& addressType) {
            const char* stage = "AdvertisementWatcher";
            try {
                std::mutex mutex;
                std::condition_variable cv;
                bool found = false;
                std::wstring targetName = WidenAscii(deviceName);

                advertisement::BluetoothLEAdvertisementWatcher watcher;
                stage = "AdvertisementWatcher::Received";
                watcher.ScanningMode(advertisement::BluetoothLEScanningMode::Active);
                auto token = watcher.Received([&](auto const&, advertisement::BluetoothLEAdvertisementReceivedEventArgs const& args) {
                    auto adv = args.Advertisement();
                    std::wstring localName = adv.LocalName().c_str();
                    bool nameMatches = !targetName.empty() && localName == targetName;
                    if (!nameMatches && !AdvertisementHasService(adv)) {
                        return;
                    }

                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        address = args.BluetoothAddress();
                        addressType = args.BluetoothAddressType();
                        found = true;
                    }
                    cv.notify_one();
                });

                stage = "AdvertisementWatcher::Start";
                watcher.Start();
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    cv.wait_for(lock, 6s, [&] { return found || !g_running.load(); });
                }
                stage = "AdvertisementWatcher::Stop";
                watcher.Stop();
                watcher.Received(token);
                if (found) {
                    LogMessage("found advertisement address=" + HexAddress(address) + " type=" + std::to_string(static_cast<int>(addressType)));
                    return ScanResult::Found;
                }
                LogMessage("advertisement not found name=" + deviceName);
                return ScanResult::NotFound;
            } catch (winrt::hresult_error const& e) {
                LogFailure(stage, e);
                return static_cast<uint32_t>(e.code()) == DeviceNotReadyHResult
                    ? ScanResult::AdapterUnavailable
                    : ScanResult::Failed;
            } catch (...) {
                LogFailure(stage);
                return ScanResult::Failed;
            }
        }

        ResolveResult ResolveCharacteristic(
            const std::string& deviceName,
            bt::BluetoothLEDevice& device,
            gatt::GattDeviceService& service,
            gatt::GattCharacteristic& characteristic
        ) {
            const char* stage = "FindDeviceAddress";
            try {
                uint64_t bluetoothAddress = 0;
                bt::BluetoothAddressType bluetoothAddressType = bt::BluetoothAddressType::Unspecified;
                MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassScanning);
                auto scanResult = FindDeviceAddress(deviceName, bluetoothAddress, bluetoothAddressType);
                if (scanResult != ScanResult::Found) {
                    return scanResult == ScanResult::AdapterUnavailable
                        ? ResolveResult::AdapterUnavailable
                        : ResolveResult::RetryScan;
                }

                stage = "BluetoothLEDevice::FromBluetoothAddressAsync";
                MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassConnecting);
                device = bt::BluetoothLEDevice::FromBluetoothAddressAsync(bluetoothAddress, bluetoothAddressType).get();
                if (!device) {
                    LogMessage("BluetoothLEDevice::FromBluetoothAddressAsync returned null");
                    ResetConnection(device, service, characteristic);
                    return ResolveResult::RetryConnect;
                }

                std::this_thread::sleep_for(500ms);

                stage = "BluetoothLEDevice::GetGattServicesAsync";
                auto serviceResult = device.GetGattServicesAsync(bt::BluetoothCacheMode::Uncached).get();
                if (serviceResult.Status() != gatt::GattCommunicationStatus::Success || serviceResult.Services().Size() == 0) {
                    LogMessage("GetGattServices status=" + std::to_string(static_cast<int>(serviceResult.Status())) + " count=" + std::to_string(serviceResult.Services().Size()));
                    ResetConnection(device, service, characteristic);
                    return ResolveResult::RetryConnect;
                }

                stage = "GattDeviceService::FindAdvancedService";
                for (auto const& item : serviceResult.Services()) {
                    if (item.Uuid() == AdvancedServiceUuid) {
                        service = item;
                        break;
                    }
                }
                if (!service) {
                    LogMessage("advanced service not found count=" + std::to_string(serviceResult.Services().Size()));
                    ResetConnection(device, service, characteristic);
                    return ResolveResult::RetryConnect;
                }

                stage = "GattDeviceService::GetCharacteristicsForUuidAsync";
                auto characteristicResult = service.GetCharacteristicsForUuidAsync(VirtualAzimuthUuid, bt::BluetoothCacheMode::Uncached).get();
                if (characteristicResult.Status() != gatt::GattCommunicationStatus::Success || characteristicResult.Characteristics().Size() == 0) {
                    LogMessage("GetCharacteristics status=" + std::to_string(static_cast<int>(characteristicResult.Status())) + " count=" + std::to_string(characteristicResult.Characteristics().Size()));
                    ResetConnection(device, service, characteristic);
                    return ResolveResult::RetryConnect;
                }

                characteristic = characteristicResult.Characteristics().GetAt(0);
                MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassConnected);
                LogMessage("connected advanced service and virtual azimuth characteristic");
                return ResolveResult::Connected;
            } catch (winrt::hresult_error const& e) {
                LogFailure(stage, e);
                MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassRetryingConnect);
                ResetConnection(device, service, characteristic);
                return ResolveResult::RetryConnect;
            } catch (...) {
                LogFailure(stage);
                MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassRetryingConnect);
                ResetConnection(device, service, characteristic);
                return ResolveResult::RetryConnect;
            }
        }

        bool WriteAzimuth(gatt::GattCharacteristic const& characteristic, float angle) {
            try {
                auto properties = static_cast<uint32_t>(characteristic.CharacteristicProperties());
                auto canWriteWithResponse = (properties & static_cast<uint32_t>(gatt::GattCharacteristicProperties::Write)) != 0;
                auto canWriteWithoutResponse = (properties & static_cast<uint32_t>(gatt::GattCharacteristicProperties::WriteWithoutResponse)) != 0;
                if (!canWriteWithResponse && !canWriteWithoutResponse) {
                    LogMessage("WriteAzimuth unsupported characteristic properties=" + std::to_string(properties));
                    return false;
                }

                uint16_t raw = static_cast<uint16_t>(NormalizeDegrees(angle) * 100.0f + 0.5f);
                streams::DataWriter writer;
                writer.ByteOrder(streams::ByteOrder::LittleEndian);
                writer.WriteUInt16(raw);
                auto option = canWriteWithResponse
                    ? gatt::GattWriteOption::WriteWithResponse
                    : gatt::GattWriteOption::WriteWithoutResponse;
                auto result = characteristic.WriteValueAsync(writer.DetachBuffer(), option).get();
                if (result != gatt::GattCommunicationStatus::Success) {
                    LogMessage("WriteAzimuth status=" + std::to_string(static_cast<int>(result)));
                    return false;
                }
                return true;
            } catch (winrt::hresult_error const& e) {
                LogFailure("WriteAzimuth", e);
                return false;
            } catch (...) {
                LogFailure("WriteAzimuth");
                return false;
            }
        }

        void WorkerMain() {
            try {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
            } catch (...) {
                return;
            }

            try {
                bt::BluetoothLEDevice device{nullptr};
                gatt::GattDeviceService service{nullptr};
                gatt::GattCharacteristic characteristic{nullptr};
                float lastSentYaw = -1000.0f;
                auto lastSendTime = std::chrono::steady_clock::time_point{};
                auto nextConnectAttempt = std::chrono::steady_clock::now() + 1s;

                while (g_running.load()) {
                    try {
                        if (!MapRenderState::externalCompassEnabled) {
                            MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassDisabled);
                            ResetConnection(device, service, characteristic);
                            lastSentYaw = -1000.0f;
                            lastSendTime = std::chrono::steady_clock::time_point{};
                            std::this_thread::sleep_for(250ms);
                            continue;
                        }

                        if (!characteristic) {
                            auto now = std::chrono::steady_clock::now();
                            if (g_wakeRequested.exchange(false) || now >= nextConnectAttempt) {
                                auto resolveResult = ResolveCharacteristic(
                                    MapRenderState::externalCompassDeviceName,
                                    device,
                                    service,
                                    characteristic
                                );
                                if (resolveResult != ResolveResult::Connected) {
                                    if (resolveResult == ResolveResult::AdapterUnavailable) {
                                        MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassRetryingScan);
                                        nextConnectAttempt = now + AdapterRetryInterval;
                                    } else if (resolveResult == ResolveResult::RetryScan) {
                                        MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassRetryingScan);
                                        nextConnectAttempt = now + StandardRetryInterval;
                                    } else {
                                        MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassRetryingConnect);
                                        nextConnectAttempt = now + StandardRetryInterval;
                                    }
                                    ResetConnection(device, service, characteristic);
                                }
                            }
                            std::this_thread::sleep_for(100ms);
                            continue;
                        }

                        auto now = std::chrono::steady_clock::now();
                        float yaw = 0.0f;
                        if (ResolveOutputAngle(yaw)) {
                            bool keepAliveDue = lastSendTime == std::chrono::steady_clock::time_point{} || now - lastSendTime >= KeepAliveInterval;
                            if (lastSentYaw < -360.0f || AngleDistance(yaw, lastSentYaw) >= MapRenderState::externalCompassMinDelta || keepAliveDue) {
                                if (WriteAzimuth(characteristic, yaw)) {
                                    lastSentYaw = yaw;
                                    lastSendTime = now;
                                } else {
                                    MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassRetryingWrite);
                                    ResetConnection(device, service, characteristic);
                                    nextConnectAttempt = std::chrono::steady_clock::now() + StandardRetryInterval;
                                }
                            }
                        } else {
                            lastSentYaw = -1000.0f;
                            lastSendTime = std::chrono::steady_clock::time_point{};
                        }

                        int intervalMs = std::clamp(MapRenderState::externalCompassIntervalMs, 20, 2000);
                        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
                    } catch (winrt::hresult_error const& e) {
                        LogFailure("WorkerMain loop", e);
                        MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassRetryingConnect);
                        ResetConnection(device, service, characteristic);
                        nextConnectAttempt = std::chrono::steady_clock::now() + StandardRetryInterval;
                        std::this_thread::sleep_for(250ms);
                    } catch (...) {
                        LogFailure("WorkerMain loop");
                        MapRenderState::externalCompassStatus.store(MapRenderState::ExternalCompassRetryingConnect);
                        ResetConnection(device, service, characteristic);
                        nextConnectAttempt = std::chrono::steady_clock::now() + StandardRetryInterval;
                        std::this_thread::sleep_for(250ms);
                    }
                }

                ResetConnection(device, service, characteristic);
            } catch (winrt::hresult_error const& e) {
                LogFailure("WorkerMain", e);
            } catch (...) {
                LogFailure("WorkerMain");
            }
        }
    }

    void Start() {
        std::lock_guard<std::mutex> lock(g_workerMutex);
        if (g_running.load()) {
            return;
        }
        g_running.store(true);
        g_worker = std::thread(WorkerMain);
    }

    void Stop() {
        {
            std::lock_guard<std::mutex> lock(g_workerMutex);
            if (!g_running.load()) {
                return;
            }
            g_running.store(false);
        }
        if (g_worker.joinable()) {
            g_worker.join();
        }
    }

    void NotifyConfigChanged() {
        g_wakeRequested.store(true);
    }

    void SetTargetPoint(const std::string& id, float x, float z, int dimensionId) {
        {
            std::lock_guard<std::mutex> lock(g_targetMutex);
            g_targetEnabled = true;
            g_targetId = id;
            g_targetX = x;
            g_targetZ = z;
            g_targetDimensionId = dimensionId;
        }
        g_wakeRequested.store(true);
    }

    void ClearTargetPoint() {
        {
            std::lock_guard<std::mutex> lock(g_targetMutex);
            g_targetEnabled = false;
            g_targetId.clear();
        }
        g_wakeRequested.store(true);
    }

    bool IsTargetPointing(const std::string& id) {
        std::lock_guard<std::mutex> lock(g_targetMutex);
        return g_targetEnabled && g_targetId == id;
    }
}
