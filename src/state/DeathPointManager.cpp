#include "state/DeathPointManager.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>

using json = nlohmann::json;

namespace DeathPointManager {
    std::vector<DeathPoint> g_deathPoints;
    std::mutex g_deathMutex;

    namespace {
        constexpr size_t MaxDeathPoints = 20;
        std::string g_deathFile;

        std::string GenerateID() {
            static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
            static std::mt19937 rng(std::random_device{}());
            static std::uniform_int_distribution<int> dist(0, (int)(sizeof(alphanum) - 2));
            std::string id;
            id.reserve(10);
            for (int i = 0; i < 10; ++i) id += alphanum[dist(rng)];
            return id;
        }

        long long NowUnixSeconds() {
            return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }

        void TrimLocked() {
            std::sort(g_deathPoints.begin(), g_deathPoints.end(), [](const DeathPoint& a, const DeathPoint& b) {
                return a.timestamp > b.timestamp;
            });
            if (g_deathPoints.size() > MaxDeathPoints) {
                g_deathPoints.resize(MaxDeathPoints);
            }
        }
    }

    void Init() {
        std::filesystem::create_directories("mods/ChiyanMap/deaths");
    }

    void SwitchWorld(const std::string& worldId) {
        {
            std::lock_guard<std::mutex> lock(g_deathMutex);
            std::filesystem::create_directories("mods/ChiyanMap/deaths");
            g_deathFile = "mods/ChiyanMap/deaths/" + worldId + ".json";
        }
        LoadDeathPoints();
    }

    void SaveDeathPoints() {
        std::lock_guard<std::mutex> lock(g_deathMutex);
        if (g_deathFile.empty()) return;

        json j = json::array();
        for (const auto& point : g_deathPoints) {
            j.push_back({
                {"id", point.id},
                {"x", point.x},
                {"y", point.y},
                {"z", point.z},
                {"dimensionId", point.dimensionId},
                {"timestamp", point.timestamp}
            });
        }

        std::ofstream out(g_deathFile);
        if (out.is_open()) out << j.dump(4);
    }

    void LoadDeathPoints() {
        std::lock_guard<std::mutex> lock(g_deathMutex);
        g_deathPoints.clear();
        if (g_deathFile.empty() || !std::filesystem::exists(g_deathFile)) return;

        std::ifstream in(g_deathFile);
        if (!in.is_open()) return;

        try {
            json j;
            in >> j;
            for (const auto& item : j) {
                DeathPoint point;
                point.id = item.value("id", GenerateID());
                point.x = item.value("x", 0);
                point.y = item.value("y", 0);
                point.z = item.value("z", 0);
                point.dimensionId = item.value("dimensionId", 0);
                point.timestamp = item.value("timestamp", 0LL);
                g_deathPoints.push_back(point);
            }
            TrimLocked();
        } catch (...) {
            g_deathPoints.clear();
        }
    }

    void AddDeathPoint(int x, int y, int z, int dimensionId) {
        {
            std::lock_guard<std::mutex> lock(g_deathMutex);
            DeathPoint point;
            point.id = GenerateID();
            point.x = x;
            point.y = y;
            point.z = z;
            point.dimensionId = dimensionId;
            point.timestamp = NowUnixSeconds();
            g_deathPoints.push_back(point);
            TrimLocked();
        }
        SaveDeathPoints();
    }

    void RemoveDeathPoint(const std::string& id) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_deathMutex);
            auto it = std::remove_if(g_deathPoints.begin(), g_deathPoints.end(), [&](const DeathPoint& point) {
                return point.id == id;
            });
            if (it != g_deathPoints.end()) {
                g_deathPoints.erase(it, g_deathPoints.end());
                changed = true;
            }
        }
        if (changed) SaveDeathPoints();
    }
}
