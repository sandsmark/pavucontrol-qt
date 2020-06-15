#pragma once

#include <QString>
#include <QIcon>

#include <pulse/proplist.h>

namespace utils {
    template<typename T>
    inline QString readProperty(const T &info, const char *key) {
        const char *value = pa_proplist_gets(info.proplist, key);
        if (!value) {
            return QString();
        }
        return QString::fromUtf8(value);
    }

    template<typename T>
    inline QString readProperty(const T &info, const QString &key) {
        return readProperty(info, key.toUtf8().constData());
    }

    QIcon iconByName(const QString &name, const QString &fallback) {
        if (name.isEmpty()) {
            return QIcon::fromTheme(fallback);
        }
        QIcon icon = QIcon::fromTheme(name);
        if (icon.isNull()) {
            return QIcon::fromTheme(fallback);
        }

        return icon;
    }

    template<typename T>
    inline QIcon findIcon(const T &info, const char *fallback) {
        static const QVector<QString> iconPropertyNames({
            PA_PROP_MEDIA_ICON_NAME,
            PA_PROP_WINDOW_ICON_NAME,
            PA_PROP_APPLICATION_ICON_NAME,
        });

        for (const QString &property : iconPropertyNames) {
            const QString value = readProperty(info, property);
            if (value.isEmpty()) {
                continue;
            }
            QIcon icon = QIcon::fromTheme(value);
            if (!icon.isNull()) {
                return icon;
            }
        }

        const QString role = readProperty(info, PA_PROP_MEDIA_ROLE);
        if (role.isEmpty()) {
            return QIcon::fromTheme(QLatin1String(fallback));
        }

        static const QHash<QString, QString> roleIcons({
            {"video", "video"},
            {"phone", "phone"},
            {"music", "audio"},
            {"game", "applications-games"},
            {"event", "dialog-information"},
        });

        if (roleIcons.contains(role)) {
            return QIcon::fromTheme(roleIcons[role]);
        }

        return QIcon::fromTheme(QLatin1String(fallback));
    }

    template<typename T>
    inline QIcon deviceIcon(const T &info) {
        static const QIcon defaultIcon = QIcon::fromTheme("audio-card");

        const QString iconName = readProperty(info, PA_PROP_DEVICE_ICON_NAME);
        if (iconName.isEmpty()) {
            return defaultIcon;
        }
        QIcon icon = QIcon::fromTheme(iconName);
        if (icon.isNull()) {
            return defaultIcon;
        }

        return icon;
    }
}

