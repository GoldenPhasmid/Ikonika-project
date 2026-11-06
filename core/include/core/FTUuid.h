#pragma once

#include <string>

std::string MakeUuid();

std::string MakeTimeOrderedUuid();

/** True for a correctly shaped lowercase version 4 UUID. Used to reject junk from outside. */
bool IsValidUuid(const std::string& Value);
