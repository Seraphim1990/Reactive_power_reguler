//#include <QCoreApplication>
//#include <QTimer>
//#include <QProcess>
//#include <QDateTime>
//
//#define NOMINMAX
//#define WIN32_LEAN_AND_MEAN
//#include <tlhelp32.h>
//#include <windows.h>
//
//
//#include "APIWorker/ApiWorker.h"
//#include "ModbusReguler/ModbusReguler.h"
//
//
//// --- Перевірка, чи запущений процес ---
//bool isProcessRunning(const QString &processName)
//{
//    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
//    if (snapshot == INVALID_HANDLE_VALUE)
//        return false;
//
//    PROCESSENTRY32W entry;
//    entry.dwSize = sizeof(entry);
//    bool found = false;
//
//    if (Process32FirstW(snapshot, &entry)) {
//        do {
//            if (_wcsicmp(processName.toStdWString().c_str(), entry.szExeFile) == 0) {
//                found = true;
//                break;
//            }
//        } while (Process32NextW(snapshot, &entry));
//    }
//
//    CloseHandle(snapshot);
//    return found;
//}
//
//// --- Функція для перевірки та запуску ResourceeController ---
//void ensureResourceControllerRunning()
//{
//    const QString exeName = "ResourceeController.exe";
//    const QString exePath = QCoreApplication::applicationDirPath() + "/../" + exeName;
//
//    if (!isProcessRunning(exeName)) {
//        qWarning() << "[Watchdog]" << exeName << "не знайдено, пробуємо запустити...";
//        bool ok = QProcess::startDetached(exePath);
//        if (ok)
//            qInfo() << "[Watchdog]" << exeName << "успішно запущено.";
//        else
//            qCritical() << "[Watchdog] Не вдалося запустити" << exePath;
//    }
//}
//
//
//int main(int argc, char *argv[])
//{
//    QCoreApplication a(argc, argv);
//
//    ApiWorker msgMenager;
//    ModbusReguler reguler;
//    msgMenager.setReguler(&reguler);
//
//    return a.exec();
//}
//

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QDebug>

#include "APIWorker/ApiWorker.h"
#include "ModbusReguler/ModbusReguler.h"

// --- Перевірка, чи запущений процес ---
bool isProcessRunning(const QString &processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    bool found = false;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(processName.toStdWString().c_str(), entry.szExeFile) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
}

// --- Запуск ResourceeController у ВЛАСНІЙ КОНСОЛІ ---
bool launchControllerInNewConsole(const QString &exePath, const QString &workingDir)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    // Формуємо повний командний рядок: "C:\path\ResourceeController.exe"
    QString cmdLine = QString("\"%1\"").arg(QDir::toNativeSeparators(exePath));
    wchar_t *cmdLineW = _wcsdup(cmdLine.toStdWString().c_str());

    // Робочий каталог (вказуємо явно)
    wchar_t *workingDirW = workingDir.isEmpty() ? nullptr : _wcsdup(workingDir.toStdWString().c_str());

    BOOL success = CreateProcessW(
        nullptr,                    // lpApplicationName — null, бо вказуємо в cmdline
        cmdLineW,                   // Командний рядок
        nullptr, nullptr,           // Без успадкування дескрипторів
        FALSE,                      // Не успадковувати handles
        CREATE_NEW_CONSOLE,         // КРИТИЧНО: НОВА КОНСОЛЬ!
        nullptr,                    // Середовище
        workingDirW,                // Робочий каталог
        &si,
        &pi
        );

    if (workingDirW) free(workingDirW);
    free(cmdLineW);

    if (success) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    } else {
        qCritical() << "[ModbusReguler] CreateProcess failed. Error:" << GetLastError();
        return false;
    }
}

// --- Основна функція ---
void ensureCoordinatorRunning()
{
    static const QString exeName = "ResourceeController.exe";

    // Визначаємо шлях
    QDir appDir = QCoreApplication::applicationDirPath();
    appDir.cdUp(); // Один рівень вгору
    const QString controllerDir = appDir.absolutePath();
    const QString exePath = appDir.filePath(exeName);

    // Перевірка існування файлу
    if (!QFile::exists(exePath)) {
        return;
    }

    // Перевірка, чи вже запущений
    if (isProcessRunning(exeName)) {
        return;
    }

    qWarning() << "[ModbusReguler] run " << exeName << "in new console...";

    if (launchControllerInNewConsole(exePath, controllerDir)) {

    } else {
        qCritical() << "[ModbusReguler] can not run" << exeName;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    ApiWorker msgManager;
    ModbusReguler reguler;
    msgManager.setReguler(&reguler);

    // --- Таймер моніторингу координатора ---
    QTimer watchdogTimer;
    QObject::connect(&watchdogTimer, &QTimer::timeout, []() {
        ensureCoordinatorRunning();
    });
    watchdogTimer.start(30'000); // перевірка кожні 30 сек

    // Перша перевірка через 2 секунди
    QTimer::singleShot(2000, []() { ensureCoordinatorRunning(); });

    return a.exec();
}
