#pragma once

#include <QObject>

class QSocketNotifier;

namespace rigpa {

// SIGTERM and SIGINT as a clean quit: systemd's stop (and Ctrl+C) becomes QCoreApplication::quit(),
// so app.exec() returns 0 and every destructor runs.
//
// Without it a TERM is not a clean exit anywhere. On the Mac the default action kills the process
// (shell status 143, no destructors). On vip, eglfs's QFbVtHandler catches TERM and INT itself and
// calls _exit(1) (qtbase 6.10, qfbvthandler.cpp, handleInt), so every `systemctl stop rigpa` logged
// a failed unit. Construct this after the QGuiApplication: eglfs installs its handlers while the
// platform plugin loads, and the later sigaction() is the one that holds. Its SIGTSTP and SIGCONT
// handling (suspend on a VT) are left as they are.
//
// The handler only writes the signal number to a socket; the quit happens in the event loop, from
// the notifier (the self-pipe pattern: nothing but write() is safe in a signal handler).
class QuitOnSignal : public QObject
{
    Q_OBJECT

public:
    explicit QuitOnSignal(QObject *parent = nullptr);
    ~QuitOnSignal() override;

    // False when the socket pair could not be made, and the signals keep their old handlers.
    bool isInstalled() const { return m_notifier != nullptr; }

signals:
    // A handled signal arrived; QCoreApplication::quit() has been called.
    void received(int signalNumber);

private:
    void readSignal();

    QSocketNotifier *m_notifier = nullptr;
};

} // namespace rigpa
