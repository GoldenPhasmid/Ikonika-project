#pragma once

#include <optional>
#include <string>

/**
 * Pulls the 11 character video id out of whatever a paste box gets handed: a watch URL, a
 * youtu.be short link, an embed or shorts URL, any of them with extra query parameters attached,
 * or the bare id typed directly. Empty on anything that isn't one of those.
 */
std::optional<std::string> ExtractYoutubeVideoId(const std::string& UrlOrId);
