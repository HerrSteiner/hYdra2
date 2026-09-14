/*
Copyright (C) 2026 Malte Steiner

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "HetTypes.hpp"

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QVector>

namespace hydra2 {

enum class HetFormat {
    CurrentText,
    LegacyBinaryLittleEndian,
    LegacyBinaryBigEndian
};

struct HetData {
    QVector<Partial> partials;
    HetFormat format = HetFormat::CurrentText;
    bool binaryHasHeader = true;
};

class HetFile final
{
public:
    static bool load(const QString& path, HetData& out, QString& error);
    static bool save(const QString& path,
                     const HetData& data,
                     HetFormat format,
                     bool binaryHasHeader,
                     QString& error);

    static bool parseBytes(const QByteArray& bytes, HetData& out, QString& error);
    static QByteArray toCurrentText(const HetData& data);
    static QByteArray toLegacyBinary(const HetData& data,
                                     HetFormat format,
                                     bool withHeader);
};

} // namespace hydra2
