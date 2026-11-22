#include "update/version.h"

#include <QStringList>

namespace update {

bool isNewerVersion(const QString& remote, const QString& current)
{
    const QStringList remoteParts = remote.split(QLatin1Char('.'));
    const QStringList currentParts = current.split(QLatin1Char('.'));
    const int count = qMax(remoteParts.size(), currentParts.size());
    for (int i = 0; i < count; ++i) {
        const int remotePart = i < remoteParts.size() ? remoteParts.at(i).toInt() : 0;
        const int currentPart = i < currentParts.size() ? currentParts.at(i).toInt() : 0;
        if (remotePart != currentPart)
            return remotePart > currentPart;
    }
    return false;
}

} // namespace update
