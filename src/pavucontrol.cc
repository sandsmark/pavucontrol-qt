/***
  This file is part of pavucontrol.

  Copyright 2006-2008 Lennart Poettering
  Copyright 2008 Sjoerd Simons <sjoerd@luon.net>

  pavucontrol is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  pavucontrol is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with pavucontrol. If not, see <https://www.gnu.org/licenses/>.
***/

#define PACKAGE_VERSION "0.1"

#include <pulse/pulseaudio.h>
#include <pulse/ext-stream-restore.h>
#include <pulse/ext-device-manager.h>

#include "pavucontrol.h"
#include "minimalstreamwidget.h"
#include "channel.h"
#include "streamwidget.h"
#include "cardwidget.h"
#include "outputwidget.h"
#include "inputdevicewidget.h"
#include "playbackwidget.h"
#include "recordingwidget.h"
#include "rolewidget.h"
#include "mainwindow.h"
#include "qtpamainloop.h"
#include <QMessageBox>
#include <QApplication>
#include <QLocale>
#include <QLibraryInfo>
#include <QTranslator>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QString>
#include <QDebug>
#include <QTabWidget>

static pa_context *context = nullptr;
static pa_mainloop_api *api = nullptr;
static int n_outstanding = 0;
static int default_tab = 0;
static bool retry = false;
static int reconnect_timeout = 1;

void show_error(const char *txt)
{
    QMessageBox::critical(nullptr, QObject::tr("Error"),
            QStringLiteral("%1: %2").arg(txt, pa_strerror(pa_context_errno(context)))
            );
    qApp->quit();
}

static void dec_outstanding(MainWindow *w)
{
    if (n_outstanding <= 0) {
        return;
    }

    if (--n_outstanding <= 0) {
        // w->get_window()->set_cursor();
        w->setConnectionState(true);
    }
}

void card_cb(pa_context *, const pa_card_info *i, int eol, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY) {
            return;
        }

        show_error(QObject::tr("Card callback failure").toUtf8().constData());
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    w->updateCard(*i);
}

static void ext_device_restore_subscribe_cb(pa_context *c, pa_device_type_t type, uint32_t idx, void *userdata);

void sink_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY) {
            return;
        }

        show_error(QObject::tr("Sink callback failure").toUtf8().constData());
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    if (w->updateOutputWidget(*i)) {
        ext_device_restore_subscribe_cb(c, PA_DEVICE_TYPE_SINK, i->index, w);
    }
}

void source_cb(pa_context *, const pa_source_info *i, int eol, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY) {
            return;
        }

        show_error(QObject::tr("Source callback failure").toUtf8().constData());
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    w->updateInputDeviceWidget(*i);
}

void sink_input_cb(pa_context *, const pa_sink_input_info *i, int eol, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY) {
            return;
        }

        show_error(QObject::tr("Sink input callback failure").toUtf8().constData());
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    w->updatePlaybackWidget(*i);
}

void source_output_cb(pa_context *, const pa_source_output_info *i, int eol, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY) {
            return;
        }

        show_error(QObject::tr("Source output callback failure").toUtf8().constData());
        return;
    }

    if (eol > 0)  {

        if (n_outstanding > 0) {
            /* At this point all notebook pages have been populated, so
             * let's open one that isn't empty */
            if (default_tab != -1) {
                if (default_tab < 1 || default_tab > w->m_notebook->count()) {
                    if (!w->m_playbackWidgets.empty()) {
                        w->m_notebook->setCurrentIndex(0);
                    } else if (!w->m_recordingWidgets.empty()) {
                        w->m_notebook->setCurrentIndex(1);
                    } else if (!w->m_inputDeviceWidgets.empty() && w->m_outputWidgets.empty()) {
                        w->m_notebook->setCurrentIndex(3);
                    } else {
                        w->m_notebook->setCurrentIndex(2);
                    }
                } else {
                    w->m_notebook->setCurrentIndex(default_tab - 1);
                }

                default_tab = -1;
            }
        }

        dec_outstanding(w);
        return;
    }

    w->updateRecordingWidget(*i);
}

void client_cb(pa_context *, const pa_client_info *i, int eol, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (eol < 0) {
        if (pa_context_errno(context) == PA_ERR_NOENTITY) {
            return;
        }

        show_error(QObject::tr("Client callback failure").toUtf8().constData());
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    w->updateClient(*i);
}

void server_info_cb(pa_context *, const pa_server_info *i, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (!i) {
        show_error(QObject::tr("Server info callback failure").toUtf8().constData());
        return;
    }

    w->updateServer(*i);
    dec_outstanding(w);
}

void ext_stream_restore_read_cb(
    pa_context *,
    const pa_ext_stream_restore_info *i,
    int eol,
    void *userdata)
{

    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (eol < 0) {
        dec_outstanding(w);
        qDebug(QObject::tr("Failed to initialize stream_restore extension: %s").toUtf8().constData(), pa_strerror(pa_context_errno(context)));
        w->deleteEventRoleWidget();
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    w->updateRole(*i);
}

static void ext_stream_restore_subscribe_cb(pa_context *c, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);
    pa_operation *o;

    if (!(o = pa_ext_stream_restore_read(c, ext_stream_restore_read_cb, w))) {
        show_error(QObject::tr("pa_ext_stream_restore_read() failed").toUtf8().constData());
        return;
    }

    pa_operation_unref(o);
}

void ext_device_restore_read_cb(
    pa_context *,
    const pa_ext_device_restore_info *i,
    int eol,
    void *userdata)
{

    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (eol < 0) {
        dec_outstanding(w);
        qDebug(QObject::tr("Failed to initialize device restore extension: %s").toUtf8().constData(), pa_strerror(pa_context_errno(context)));
        return;
    }

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    /* Do something with a widget when this part is written */
    w->updateDeviceInfo(*i);
}

static void ext_device_restore_subscribe_cb(pa_context *c, pa_device_type_t type, uint32_t idx, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);
    pa_operation *o;

    if (type != PA_DEVICE_TYPE_SINK) {
        return;
    }

    if (!(o = pa_ext_device_restore_read_formats(c, type, idx, ext_device_restore_read_cb, w))) {
        show_error(QObject::tr("pa_ext_device_restore_read_sink_formats() failed").toUtf8().constData());
        return;
    }

    pa_operation_unref(o);
}

void ext_device_manager_read_cb(
    pa_context *,
    const pa_ext_device_manager_info *,
    int eol,
    void *userdata)
{

    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (eol < 0) {
        dec_outstanding(w);
        qDebug(QObject::tr("Failed to initialize device manager extension: %s").toUtf8().constData(), pa_strerror(pa_context_errno(context)));
        return;
    }

    w->m_canRenameDevices = true;

    if (eol > 0) {
        dec_outstanding(w);
        return;
    }

    /* Do something with a widget when this part is written */
}

static void ext_device_manager_subscribe_cb(pa_context *c, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);
    pa_operation *o;

    if (!(o = pa_ext_device_manager_read(c, ext_device_manager_read_cb, w))) {
        show_error(QObject::tr("pa_ext_device_manager_read() failed").toUtf8().constData());
        return;
    }

    pa_operation_unref(o);
}

void subscribe_cb(pa_context *c, pa_subscription_event_type_t t, uint32_t index, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);

    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
    case PA_SUBSCRIPTION_EVENT_SINK:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
            w->removeOutputWidget(index);
        } else {
            pa_operation *o;

            if (!(o = pa_context_get_sink_info_by_index(c, index, sink_cb, w))) {
                show_error(QObject::tr("pa_context_get_sink_info_by_index() failed").toUtf8().constData());
                return;
            }

            pa_operation_unref(o);
        }

        break;

    case PA_SUBSCRIPTION_EVENT_SOURCE:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
            w->removeInputDevice(index);
        } else {
            pa_operation *o;

            if (!(o = pa_context_get_source_info_by_index(c, index, source_cb, w))) {
                show_error(QObject::tr("pa_context_get_source_info_by_index() failed").toUtf8().constData());
                return;
            }

            pa_operation_unref(o);
        }

        break;

    case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
            w->removePlaybackWidget(index);
        } else {
            pa_operation *o;

            if (!(o = pa_context_get_sink_input_info(c, index, sink_input_cb, w))) {
                show_error(QObject::tr("pa_context_get_sink_input_info() failed").toUtf8().constData());
                return;
            }

            pa_operation_unref(o);
        }

        break;

    case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
            w->removeRecordingWidget(index);
        } else {
            pa_operation *o;

            if (!(o = pa_context_get_source_output_info(c, index, source_output_cb, w))) {
                show_error(QObject::tr("pa_context_get_sink_input_info() failed").toUtf8().constData());
                return;
            }

            pa_operation_unref(o);
        }

        break;

    case PA_SUBSCRIPTION_EVENT_CLIENT:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
            w->removeClient(index);
        } else {
            pa_operation *o;

            if (!(o = pa_context_get_client_info(c, index, client_cb, w))) {
                show_error(QObject::tr("pa_context_get_client_info() failed").toUtf8().constData());
                return;
            }

            pa_operation_unref(o);
        }

        break;

    case PA_SUBSCRIPTION_EVENT_SERVER: {
        pa_operation *o;

        if (!(o = pa_context_get_server_info(c, server_info_cb, w))) {
            show_error(QObject::tr("pa_context_get_server_info() failed").toUtf8().constData());
            return;
        }

        pa_operation_unref(o);
    }
    break;

    case PA_SUBSCRIPTION_EVENT_CARD:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
            w->removeCard(index);
        } else {
            pa_operation *o;

            if (!(o = pa_context_get_card_info_by_index(c, index, card_cb, w))) {
                show_error(QObject::tr("pa_context_get_card_info_by_index() failed").toUtf8().constData());
                return;
            }

            pa_operation_unref(o);
        }

        break;

    }
}

static void deviceManagerLoadedCb(pa_context *context, uint32_t idx, void *userdata)
{
    pa_operation *operation = pa_ext_device_manager_read(context, ext_device_manager_read_cb, userdata);

    if (!operation) {
        qDebug(QObject::tr("Failed to initialize device manager extension: %s").toUtf8().constData(), pa_strerror(pa_context_errno(context)));
        return;
    }

    pa_operation_unref(operation);
    n_outstanding++;

    pa_ext_device_manager_set_subscribe_cb(context, ext_device_manager_subscribe_cb, userdata);

    operation = pa_ext_device_manager_subscribe(context, 1, nullptr, nullptr);
    if (operation) {
        pa_operation_unref(operation);
    }

}

/* Forward Declaration */
void connect_to_pulse(MainWindow *w);

void context_state_callback(pa_context *c, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);

    Q_ASSERT(c);

    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_UNCONNECTED:
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
        break;

    case PA_CONTEXT_READY: {
        pa_operation *o;

        reconnect_timeout = 1;

        /* Create event widget immediately so it's first in the list */
        w->createEventRoleWidget();

        pa_context_set_subscribe_callback(c, subscribe_cb, w);

        if (!(o = pa_context_subscribe(c, (pa_subscription_mask_t)
                                       (PA_SUBSCRIPTION_MASK_SINK |
                                        PA_SUBSCRIPTION_MASK_SOURCE |
                                        PA_SUBSCRIPTION_MASK_SINK_INPUT |
                                        PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT |
                                        PA_SUBSCRIPTION_MASK_CLIENT |
                                        PA_SUBSCRIPTION_MASK_SERVER |
                                        PA_SUBSCRIPTION_MASK_CARD), nullptr, nullptr))) {
            show_error(QObject::tr("pa_context_subscribe() failed").toUtf8().constData());
            return;
        }

        pa_operation_unref(o);

        /* Keep track of the outstanding callbacks for UI tweaks */
        n_outstanding = 0;

        if (!(o = pa_context_get_server_info(c, server_info_cb, w))) {
            show_error(QObject::tr("pa_context_get_server_info() failed").toUtf8().constData());
            return;
        }

        pa_operation_unref(o);
        n_outstanding++;

        if (!(o = pa_context_get_client_info_list(c, client_cb, w))) {
            show_error(QObject::tr("pa_context_client_info_list() failed").toUtf8().constData());
            return;
        }

        pa_operation_unref(o);
        n_outstanding++;

        if (!(o = pa_context_get_card_info_list(c, card_cb, w))) {
            show_error(QObject::tr("pa_context_get_card_info_list() failed").toUtf8().constData());
            return;
        }

        pa_operation_unref(o);
        n_outstanding++;

        if (!(o = pa_context_get_sink_info_list(c, sink_cb, w))) {
            show_error(QObject::tr("pa_context_get_sink_info_list() failed").toUtf8().constData());
            return;
        }

        pa_operation_unref(o);
        n_outstanding++;

        if (!(o = pa_context_get_source_info_list(c, source_cb, w))) {
            show_error(QObject::tr("pa_context_get_source_info_list() failed").toUtf8().constData());
            return;
        }

        pa_operation_unref(o);
        n_outstanding++;

        if (!(o = pa_context_get_sink_input_info_list(c, sink_input_cb, w))) {
            show_error(QObject::tr("pa_context_get_sink_input_info_list() failed").toUtf8().constData());
            return;
        }

        pa_operation_unref(o);
        n_outstanding++;

        if (!(o = pa_context_get_source_output_info_list(c, source_output_cb, w))) {
            show_error(QObject::tr("pa_context_get_source_output_info_list() failed").toUtf8().constData());
            return;
        }

        pa_operation_unref(o);
        n_outstanding++;

        /* These calls are not always supported */
        if ((o = pa_ext_stream_restore_read(c, ext_stream_restore_read_cb, w))) {
            pa_operation_unref(o);
            n_outstanding++;

            pa_ext_stream_restore_set_subscribe_cb(c, ext_stream_restore_subscribe_cb, w);

            if ((o = pa_ext_stream_restore_subscribe(c, 1, nullptr, nullptr))) {
                pa_operation_unref(o);
            }

        } else {
            qDebug(QObject::tr("Failed to initialize stream_restore extension: %s").toUtf8().constData(), pa_strerror(pa_context_errno(context)));
        }

        /* TODO Change this to just the test function */
        if ((o = pa_ext_device_restore_read_formats_all(c, ext_device_restore_read_cb, w))) {
            pa_operation_unref(o);
            n_outstanding++;

            pa_ext_device_restore_set_subscribe_cb(c, ext_device_restore_subscribe_cb, w);

            if ((o = pa_ext_device_restore_subscribe(c, 1, nullptr, nullptr))) {
                pa_operation_unref(o);
            }

        } else {
            qDebug(QObject::tr("Failed to initialize device restore extension: %s").toUtf8().constData(), pa_strerror(pa_context_errno(context)));
        }

        pa_operation *operation = pa_ext_device_manager_read(context, ext_device_manager_read_cb, userdata);
        if (!operation) {
            // Device manager not available, attempt to load it
            operation = pa_context_load_module(c, "module-device-manager", "", deviceManagerLoadedCb, w);
            if (operation) {
                pa_operation_unref(operation);
            } else {
                qDebug(QObject::tr("Failed to load device manager extension: %s").toUtf8().constData(), pa_strerror(pa_context_errno(context)));
            }
        } else {
            pa_operation_unref(operation);
        }


        break;
    }

    case PA_CONTEXT_FAILED:
        w->setConnectionState(false);

        w->removeAllWidgets();
        w->updateDeviceVisibility();
        pa_context_unref(context);
        context = nullptr;

        if (reconnect_timeout > 0) {
            qDebug("%s", QObject::tr("Connection failed, attempting reconnect").toUtf8().constData());
            QTimer::singleShot(reconnect_timeout, qApp, [w]() {
                connect_to_pulse(w);
            });
        }

        return;

    case PA_CONTEXT_TERMINATED:
    default:
        qApp->quit();
        return;
    }
}

pa_context *get_context()
{
    return context;
}

void connect_to_pulse(MainWindow *w)
{
    if (context) {
        return;
    }

    pa_proplist *proplist = pa_proplist_new();
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME, QObject::tr("PulseAudio Volume Control").toUtf8().constData());
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_ID, "org.PulseAudio.pavucontrol");
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_ICON_NAME, "audio-card");
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_VERSION, PACKAGE_VERSION);

    context = pa_context_new_with_proplist(api, nullptr, proplist);
    Q_ASSERT(context);

    pa_proplist_free(proplist);

    pa_context_set_state_callback(context, context_state_callback, w);

    w->setConnectingMessage();

    if (pa_context_connect(context, nullptr, PA_CONTEXT_NOFAIL, nullptr) < 0) {
        if (pa_context_errno(context) == PA_ERR_INVALID) {
            w->setConnectingMessage(QObject::tr("Connection to PulseAudio failed. Automatic retry in 5s<br><br>"
                                                "In this case this is likely because PULSE_SERVER in the Environment/X11 Root Window Properties<br>"
                                                "or default-server in client.conf is misconfigured.<br>"
                                                "This situation can also arise when PulseAudio crashed and left stale details in the X11 Root Window.<br>"
                                                "If this is the case, then PulseAudio should autospawn again, or if this is not configured you should<br>"
                                                "run start-pulseaudio-x11 manually.").toUtf8().constData());
            reconnect_timeout = 5;
        } else {
            if (!retry) {
                reconnect_timeout = -1;
                qApp->quit();
            } else {
                qDebug("%s", QObject::tr("Connection failed, attempting reconnect").toUtf8().constData());
                reconnect_timeout = 5;
                QTimer::singleShot(reconnect_timeout, qApp, [w]() {
                    connect_to_pulse(w);
                });
            }
        }
    }
}

int main(int argc, char *argv[])
{

    signal(SIGPIPE, SIG_IGN);
    qputenv("QT_NO_GLIB", "1");

    QApplication app(argc, argv);

    app.setOrganizationName(QStringLiteral("pavucontrol-qt"));
    app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QString locale = QLocale::system().name();
    QTranslator qtTranslator;

    if (qtTranslator.load(QStringLiteral("qt_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        qApp->installTranslator(&qtTranslator);
    }

    QTranslator appTranslator;

    if (appTranslator.load(QStringLiteral("pavucontrol-qt_") + locale, QStringLiteral(PAVUCONTROL_QT_DATA_DIR) + QStringLiteral("/translations"))) {
        qApp->installTranslator(&appTranslator);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("PulseAudio Volume Control"));
    parser.addHelpOption();

    const QString VERINFO = QStringLiteral(PAVUCONTROLQT_VERSION
                                           "\nQt        " QT_VERSION_STR);
    app.setApplicationVersion(VERINFO);
    parser.addVersionOption();

    QCommandLineOption tabOption(QStringList() << QStringLiteral("tab") << QStringLiteral("t"), QObject::tr("Select a specific tab on load."), QStringLiteral("tab"));
    parser.addOption(tabOption);

    QCommandLineOption retryOption(QStringList() << QStringLiteral("retry") << QStringLiteral("r"), QObject::tr("Retry forever if pa quits (every 5 seconds)."));
    parser.addOption(retryOption);

    QCommandLineOption maximizeOption(QStringList() << QStringLiteral("maximize") << QStringLiteral("m"), QObject::tr("Maximize the window."));
    parser.addOption(maximizeOption);

    parser.process(app);
    default_tab = parser.value(tabOption).toInt();
    retry = parser.isSet(retryOption);

    // ca_context_set_driver(ca_gtk_context_get(), "pulse");

    MainWindow *mainWindow = new MainWindow(nullptr);

    if (parser.isSet(maximizeOption)) {
        mainWindow->showMaximized();
    }

    QtPaMainLoop mainloop;
    api = &mainloop.pa_vtable;

    connect_to_pulse(mainWindow);

    if (reconnect_timeout >= 0) {
        mainWindow->show();
        app.exec();
    }

    if (reconnect_timeout < 0) {
        show_error(QObject::tr("Fatal Error: Unable to connect to PulseAudio").toUtf8().constData());
    }

    delete mainWindow;

    if (context) {
        pa_context_unref(context);
    }

    return 0;
}
