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
