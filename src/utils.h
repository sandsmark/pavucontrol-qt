#pragma once

#include <QString>
#include <QSet>
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
    inline bool heuristicIsHeadset(const T &info) {
        const QString bus = readProperty(info, PA_PROP_DEVICE_BUS);

        if (bus != QLatin1String("usb")) {
            return false;
        }

        const QString vendor = readProperty(info, PA_PROP_DEVICE_VENDOR_ID);

        // Vendors that only produce USB headsets, not other kinds of USB audio cards

        // Logitech
        if (vendor == QLatin1String("046d")) {
            return true;
        }
        // Kingston
        if (vendor == QLatin1String("0951")) {
            return true;
        }

        return false;
    }

    template<typename T>
    inline bool heuristicIsDisplay(const T &info) {
        const QString description = utils::readProperty(info, PA_PROP_DEVICE_DESCRIPTION);

        if (description.contains(QLatin1String("hdmi"), Qt::CaseInsensitive)) {
            // Logitech USB headset
            return true;
        }

        return false;
    }


    template<typename T>
    inline QIcon deviceIcon(const T &info) {
        static const QIcon defaultIcon = QIcon::fromTheme("audio-card");
        static const QIcon headsetIcon = QIcon::fromTheme("audio-headset");
        static const QIcon hdmiIcon = QIcon::fromTheme("tv");

        // Trust our own heuristics more than pulseaudio/udev
        if (heuristicIsHeadset(info)) {
            return headsetIcon;
        }
        if (heuristicIsDisplay(info)) {
            return hdmiIcon;
        }

        const QString iconName = readProperty(info, PA_PROP_DEVICE_ICON_NAME);
        if (iconName.isEmpty()) {
            if (heuristicIsHeadset(info)) {
                return headsetIcon;
            } else {
                return defaultIcon;
            }
        }

        QIcon icon = QIcon::fromTheme(iconName);
        if (icon.isNull()) {
            return defaultIcon;
        }

        return icon;
    }

    // pipewire is missing most of the "proper" properties, so we hardcode this instead
    static const QSet<QString> mixers({
            "org.PulseAudio.pavucontrol",
            "org.gnome.VolumeControl",
            "org.kde.kmixd"
            });

    template<typename T>
    inline bool shouldIgnoreApp(const T &info) {
        if (mixers.contains(utils::readProperty(info, PA_PROP_APPLICATION_ID))) {
            return true;
        }

        // Handled by system event thing
        // Does not work with pipewire, hence the test above as well
        if (utils::readProperty(info, "module-stream-restore.id") == "sink-input-by-media-role:event") {
            return true;
        }
        // Empty from pipewire
        if (utils::readProperty(info, PA_PROP_MEDIA_ROLE) == "event") {
            return true;
        }

        return false;
    }
}

