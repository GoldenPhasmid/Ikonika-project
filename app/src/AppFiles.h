#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

std::optional<QByteArray> ReadWholeFile(const QString& Path);

void RemoveDatabaseFiles(const QString& Path);
