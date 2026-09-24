#include "ModbusReply.h"
#include <QDebug> // Для виведення відлагоджувальних повідомлень


ModbusReply::ModbusReply(ModbusDataUnit &unit):
    QObject{nullptr},
    m_valid(false),
    m_error(NoError),
    m_workType(Unknown), // Ініціалізуємо workType
    m_dataUnit(&unit)    // Ініціалізуємо m_dataUnit тут
{
    if (!unit.isValid()) {
        inValid();
        m_error = DataUnitInvalid;
    }
    else {
        valid();
        m_dataList.resize(m_dataUnit->lenght());
    }
}

void ModbusReply::read()
{
    m_workType = Read;
}

void ModbusReply::write()
{
    m_workType = Write;
}

ModbusReply::Errors ModbusReply::error()
{
    return m_error;
}

bool ModbusReply::isValid()
{
    return m_valid;
}

void ModbusReply::valid()
{
    m_valid = true;
}

void ModbusReply::inValid()
{
    m_valid = false;
}

void ModbusReply::notReaded()
{
    m_error = DeviceNotResponce;
    inValid();
    emit finished();
}



const std::vector<int> &ModbusReply::data()
{
    return m_dataList;
}

void ModbusReply::takeData(const QByteArray &response)
{
    std::fill(m_dataList.begin(), m_dataList.end(), 0); // Очищаємо попередні дані
    m_error = NoError; // Скидаємо помилку перед парсингом

    // Перевірка, чи відповідь достатньо довга
    if (response.isEmpty()) {
        inValid();
        m_error = InvalidResponseLength;
        emit finished();
        return;
    }

    if (m_workType == Read) {
        vasRead(response);
    }
    else if (m_workType == Write) {
        vasWrite(response);
    }
    else {
        m_error = UnkownWorkType;
        inValid();
        // emit finished(); // finished буде викликано в vasRead/vasWrite
        return;
    }

    emit finished(); // Емітуємо finished після завершення парсингу
}

void ModbusReply::vasRead(const QByteArray &response)
{
    ModbusDataUnit::RegisterStatus statusRegister = m_dataUnit->regType();

    char receivedCommand = response[0];
    char expectedCommand = m_dataUnit->getReadCommand(); // Використовуємо getReadCommand()

    if (receivedCommand != expectedCommand) {
        // Обробка винятку для помилки Modbus (function code + 0x80)
        if (receivedCommand == (expectedCommand | 0x80)) {
            if (response.size() >= 2) {
                // qWarning() << "Modbus exception: " << (int)response[1];
                m_error = UnknownError; // Можна додати більш специфічні коди помилок Modbus
            } else {
                m_error = InvalidResponseLength; // Недостатня довжина для коду винятку
            }
        } else {
            m_error = InvalidResponseCommand;
        }
        inValid();
        return;
    }

    switch (statusRegister) {
    case ModbusDataUnit::HoldingRegisters:
        holdingRegistersRead(response);
        break;
    case ModbusDataUnit::InputRegisters:
        inputRegistersRead(response);
        break;
    case ModbusDataUnit::Coils:
        coilsRead(response);
        break;
    case ModbusDataUnit::DiscreteInputs:
        discreteInputsRead(response);
        break;
    default:
        inValid();
        m_error = UnknownError;
        break;
    }
}

void ModbusReply::vasWrite(const QByteArray &response)
{
    ModbusDataUnit::RegisterStatus statusRegister = m_dataUnit->regType();

    char receivedCommand = response[0];
    char expectedCommand = m_dataUnit->getWriteCommand(); // Використовуємо getWriteCommand()

    if (receivedCommand != expectedCommand) {
        // Обробка винятку для помилки Modbus (function code + 0x80)
        if (receivedCommand == (expectedCommand | 0x80)) {
            if (response.size() >= 2) {
                // qWarning() << "Modbus exception on write: " << (int)response[1];
                m_error = UnknownError;
            } else {
                m_error = InvalidResponseLength;
            }
        } else {
            m_error = InvalidResponseCommand;
        }
        inValid();
        return;
    }

    // Перевірка формату відповіді на запис
    // Для запису одного регістра/койла (0x05, 0x06) - відповідь є ехом запиту
    // Для запису множини (0x0F, 0x10) - відповідь містить адресу і кількість
    int expectedLength = 0;
    if (m_dataUnit->lenght() == 1) { // Запис одного регістра/койла (0x05, 0x06)
        expectedLength = 1 + 2 + 2; // Команда + адреса (2 байти) + значення (2 байти)
    } else { // Запис множини регістра/койла (0x0F, 0x10)
        expectedLength = 1 + 2 + 2; // Команда + адреса (2 байти) + кількість (2 байти)
    }

    if (response.size() < expectedLength) {
        inValid();
        m_error = InvalidResponseLength;
        return;
    }

    // Додаткові перевірки, якщо потрібно (наприклад, чи співпадають адреси)
    quint16 receivedAddr = (static_cast<quint8>(response[1]) << 8) | static_cast<quint8>(response[2]);
    if (receivedAddr != m_dataUnit->regAddr()) { // Припустимо, у ModbusDataUnit є regAddr()
        m_error = UnknownError; // Або нова помилка: MismatchAddress
        inValid();
        return;
    }

    if (m_dataUnit->lenght() > 1) { // Для множинного запису перевіряємо кількість
        quint16 receivedLength = (static_cast<quint8>(response[3]) << 8) | static_cast<quint8>(response[4]);
        if (receivedLength != m_dataUnit->lenght()) {
            m_error = UnknownError; // Або нова помилка: MismatchLength
            inValid();
            return;
        }
    }

    // Якщо все дійшло до цього моменту, відповідь на запис вважається успішною
    valid(); // Встановлюємо відповідь як валідну
    // Для запису ми зазвичай не "парсимо" дані в m_dataList, оскільки відповідь є підтвердженням, а не новими даними.
    // Якщо потрібно, можна записати 1 або кількість записаних елементів, щоб сигналізувати про успіх.
    // m_dataList[0] = 1; // Наприклад, якщо успішно записано.
}

void ModbusReply::holdingRegistersRead(const QByteArray &response)
{
    // Мінімальна довжина відповіді: команда (1 байт) + кількість байтів (1 байт) + дані (2 * кількість регістрів)
    int expectedDataByteCount = m_dataUnit->lenght() * 2;
    int expectedResponseLength = 1 + 1 + expectedDataByteCount;

    if (response.size() < expectedResponseLength || static_cast<quint8>(response[1]) != expectedDataByteCount) {
        inValid();
        m_error = InvalidResponseLength;
        return;
    }

    // Заповнюємо m_dataList
    for (int i = 0; i < m_dataUnit->lenght(); ++i) {
        quint16 value = (static_cast<quint8>(response[2 + i * 2]) << 8) |
                        static_cast<quint8>(response[2 + i * 2 + 1]);
        m_dataList[i] = value;
    }
    valid();
}

void ModbusReply::inputRegistersRead(const QByteArray &response)
{
    // Логіка ідентична holdingRegistersRead, оскільки обидва працюють з 16-бітними регістрами
    holdingRegistersRead(response);
}

void ModbusReply::coilsRead(const QByteArray &response)
{
    // Для котушок (Coils) і дискретних входів (Discrete Inputs)
    // Відповідь: Команда (1 байт) + Кількість байтів (1 байт) + Дані (кількість байтів з пакованими бітами)
    int expectedByteCount = (m_dataUnit->lenght() + 7) / 8; // Кількість байтів, необхідних для бітів
    int expectedResponseLength = 1 + 1 + expectedByteCount;

    if (response.size() < expectedResponseLength || static_cast<quint8>(response[1]) != expectedByteCount) {
        inValid();
        m_error = InvalidResponseLength;
        return;
    }

    for (int i = 0; i < m_dataUnit->lenght(); ++i) {
        int byteIndex = i / 8;
        int bitIndex = i % 8;
        bool bitValue = (response[2 + byteIndex] >> bitIndex) & 0x01;
        m_dataList[i] = bitValue ? 1 : 0;
    }
    valid();
}

void ModbusReply::discreteInputsRead(const QByteArray &response)
{
    // Логіка ідентична coilsRead
    coilsRead(response);
}

// Методи для запису (зараз просто заглушки, оскільки основна перевірка вже в vasWrite)
// Їх можна розширити, якщо для різних типів запису потрібна різна обробка даних (наприклад, якщо відповідь містить не тільки ехо)
void ModbusReply::holdingRegistersWrite(const QByteArray &response)
{
    // Вже оброблено в vasWrite
    Q_UNUSED(response);
}

void ModbusReply::coilsWrite(const QByteArray &response)
{
    // Вже оброблено в vasWrite
    Q_UNUSED(response);
}
