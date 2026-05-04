#pragma once

#include <Windows.h>
#include <cmath>
#include <string>
#include <DirectXMath.h>

#include "../sdk/PluginGameData.h"

namespace AutoReflex { namespace Game {

// ---------------------------------------------------------------------------
// 2D vector helper
// ---------------------------------------------------------------------------
struct Vector2f {
    float x = 0.0f, y = 0.0f;
    bool operator==(const Vector2f& o) const { return x == o.x && y == o.y; }
};

// ---------------------------------------------------------------------------
// Rarity bitmask flags
// ---------------------------------------------------------------------------
enum MonsterRarityFlag : int {
    RarityNormal = 1 << 0,   // 1
    RarityMagic  = 1 << 1,   // 2
    RarityRare   = 1 << 2,   // 4
    RarityUnique = 1 << 3,   // 8
    RarityAny    = 0xF
};

// ---------------------------------------------------------------------------
// ScreenToGrid
// Converts a Windows screen-position (pixels) to game grid coordinates using
// the inverse of the WorldToScreenMatrix from the snapshot.
// Returns {0,0} if the matrix is not invertible or the conversion fails.
// ---------------------------------------------------------------------------
inline Vector2f ScreenToGrid(POINT screenPt, const PluginSDK::PluginGameSnapshot& snap)
{
    const auto& m = snap.WorldToScreenMatrix;

    // Load the row-major XMFLOAT4X4 into an XMMATRIX
    DirectX::XMMATRIX mat = DirectX::XMLoadFloat4x4(&m);

    // XMMatrixInverse returns the inverse matrix; also outputs determinant
    DirectX::XMVECTOR det;
    DirectX::CXMMATRIX inv = DirectX::XMMatrixInverse(&det, mat);

    // Check determinant magnitude to ensure the matrix is actually invertible
    float detVal = DirectX::XMVectorGetX(det);
    if (std::fabsf(detVal) < 1e-6f) {
        return {0.0f, 0.0f};
    }

    // Homogeneous screen point: normalize to NDC then unproject
    if (snap.ScreenWidth <= 0 || snap.ScreenHeight <= 0) {
        return {0.0f, 0.0f};
    }

    float nx = (2.0f * screenPt.x / snap.ScreenWidth) - 1.0f;
    float ny = 1.0f - (2.0f * screenPt.y / snap.ScreenHeight); // flip Y

    // We don't know the depth, so use Z=0.5 (middle of NDC depth range).
    // This gives an approximate grid position on the camera's mid-plane.
    float ndcZ = 0.5f;

    // Transform NDC point by inverse matrix
    DirectX::XMVECTOR pt = DirectX::XMVectorSet(nx, ny, ndcZ, 1.0f);
    DirectX::XMVECTOR world = DirectX::XMVector3TransformCoord(pt, inv);

    DirectX::XMFLOAT3 worldPos;
    DirectX::XMStoreFloat3(&worldPos, world);

    // Convert world position to grid using WorldToGridConvertor
    float scale = snap.WorldToGridConvertor;
    if (scale <= 0.0f) {
        return {0.0f, 0.0f};
    }

    return {
        worldPos.x / scale,
        worldPos.y / scale
    };
}

// ---------------------------------------------------------------------------
// Euclidean distance on 2D grid coords
// ---------------------------------------------------------------------------
inline float GridDistance(float ax, float ay, float bx, float by)
{
    float dx = ax - bx;
    float dy = ay - by;
    return std::sqrtf(dx * dx + dy * dy);
}

// ---------------------------------------------------------------------------
// Case-insensitive substring search (C++17 compatible, no string_view)
// ---------------------------------------------------------------------------
inline bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;

    auto lo = [](char c) -> char {
        return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    };

    for (size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (lo(haystack[i + j]) != lo(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Narrow wstring to string (lossy, replaces unmappable chars with '?')
// ---------------------------------------------------------------------------
inline std::string WStringToString(const std::wstring& ws)
{
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(),
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        // Fallback: simple truncation of high bits
        std::string out(ws.size(), ' ');
        for (size_t i = 0; i < ws.size(); ++i)
            out[i] = (ws[i] < 128) ? (char)ws[i] : '?';
        return out;
    }
    std::string out(len, ' ');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(),
                        out.data(), len, nullptr, nullptr);
    return out;
}

}} // namespace AutoReflex::Game