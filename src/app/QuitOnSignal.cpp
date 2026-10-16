#include "QuitOnSignal.h"

#include <QCoreApplication>
#include <QSocketNotifier>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace rigpa {

namespace {

// [0] is written by the handler, [1] read by the notifier. One process, one pair.
int g_fds[2] = {-1, -1};

void handler(int signalNumber)
{
    const char c = char(signalNumber);
    [[maybe_unused]] const auto n = ::write(g_fds[0], &c, 1);
}

constexpr int kSignals[] = {SIGTERM, SIGINT};

} // namespace

QuitOnSignal::QuitOnSignal(QObject *parent)
    : QObject(parent)
{
    if (g_fds[0] != -1 || ::socketpair(AF_UNIX, SOCK_STREAM, 0, g_fds) != 0) {
        qWarning("rigpa: cannot handle SIGTERM: socketpair failed or already in use");
        return;
    }
    m_notifier = new QSocketNotifier(g_fds[1], QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &QuitOnSignal::readSignal);

    struct sigaction sa = {};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    for (const int s : kSignals)
        ::sigaction(s, &sa, nullptr);
}

QuitOnSignal::~QuitOnSignal()
{
    if (!m_notifier)
        return;
    struct sigaction sa = {};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    for (const int s : kSignals)
        ::sigaction(s, &sa, nullptr);
    ::close(g_fds[0]);
    ::close(g_fds[1]);
    g_fds[0] = g_fds[1] = -1;
}

void QuitOnSignal::readSignal()
{
    char c = 0;
    if (::read(g_fds[1], &c, 1) != 1)
        return;
    QCoreApplication::quit();
    emit received(int(c));
}

} // namespace rigpa
