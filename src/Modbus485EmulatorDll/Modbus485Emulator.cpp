#include "Modbus485Emulator.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace
{
constexpr std::size_t RegisterCount = 10000;
constexpr std::uint8_t ExceptionIllegalFunction = 0x01;
constexpr std::uint8_t ExceptionIllegalDataAddress = 0x02;
constexpr std::uint8_t ExceptionIllegalDataValue = 0x03;

std::mutex g_lock;
std::array<std::uint16_t, RegisterCount> g_holding{};
std::array<std::uint16_t, RegisterCount> g_input{};
std::uint8_t g_unitId = 1;
char g_lastError[160] = "OK";

void set_error(const char* message)
{
    std::strncpy(g_lastError, message, sizeof(g_lastError) - 1);
    g_lastError[sizeof(g_lastError) - 1] = '\0';
}

std::uint16_t read_be16(const unsigned char* data)
{
    return static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

void write_be16(unsigned char* data, std::uint16_t value)
{
    data[0] = static_cast<unsigned char>((value >> 8) & 0xFF);
    data[1] = static_cast<unsigned char>(value & 0xFF);
}

std::uint16_t crc16_modbus(const unsigned char* data, int length)
{
    std::uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
        {
            const bool lsb = (crc & 0x0001) != 0;
            crc >>= 1;
            if (lsb)
            {
                crc ^= 0xA001;
            }
        }
    }

    return crc;
}

bool has_valid_crc(const unsigned char* frame, int length)
{
    if (length < 4)
    {
        return false;
    }

    const auto actual = static_cast<std::uint16_t>(frame[length - 2] | (frame[length - 1] << 8));
    return crc16_modbus(frame, length - 2) == actual;
}

int append_crc(unsigned char* frame, int length, int capacity)
{
    if (length + 2 > capacity)
    {
        set_error("response buffer too small for CRC");
        return -1;
    }

    const auto crc = crc16_modbus(frame, length);
    frame[length] = static_cast<unsigned char>(crc & 0xFF);
    frame[length + 1] = static_cast<unsigned char>((crc >> 8) & 0xFF);
    return length + 2;
}

int exception_response(std::uint8_t functionCode, std::uint8_t exceptionCode, unsigned char* response, int capacity)
{
    if (capacity < 5)
    {
        set_error("response buffer too small for exception");
        return -1;
    }

    response[0] = g_unitId;
    response[1] = static_cast<unsigned char>(functionCode | 0x80);
    response[2] = exceptionCode;
    return append_crc(response, 3, capacity);
}

bool valid_range(std::uint16_t address, std::uint16_t quantity)
{
    return quantity > 0 &&
        quantity <= 125 &&
        address < RegisterCount &&
        static_cast<std::size_t>(address) + quantity <= RegisterCount;
}

int read_registers(
    const std::array<std::uint16_t, RegisterCount>& registers,
    std::uint8_t functionCode,
    std::uint16_t address,
    std::uint16_t quantity,
    unsigned char* response,
    int capacity)
{
    if (!valid_range(address, quantity))
    {
        return exception_response(functionCode, ExceptionIllegalDataAddress, response, capacity);
    }

    const int payloadLength = 3 + (quantity * 2);
    if (payloadLength + 2 > capacity)
    {
        set_error("response buffer too small");
        return -1;
    }

    response[0] = g_unitId;
    response[1] = functionCode;
    response[2] = static_cast<unsigned char>(quantity * 2);

    for (std::uint16_t i = 0; i < quantity; ++i)
    {
        write_be16(&response[3 + (i * 2)], registers[address + i]);
    }

    return append_crc(response, payloadLength, capacity);
}
}

int __stdcall M485_Init(unsigned char unitId)
{
    std::lock_guard<std::mutex> guard(g_lock);
    g_unitId = unitId == 0 ? 1 : unitId;
    g_holding.fill(0);
    g_input.fill(0);

    for (std::uint16_t i = 0; i < 10; ++i)
    {
        g_holding[i] = static_cast<std::uint16_t>(100 + i);
        g_input[i] = static_cast<std::uint16_t>(200 + i);
    }

    set_error("OK");
    return 1;
}

int __stdcall M485_Reset(void)
{
    return M485_Init(g_unitId);
}

int __stdcall M485_SetHolding(unsigned short address, unsigned short value)
{
    std::lock_guard<std::mutex> guard(g_lock);
    if (address >= RegisterCount)
    {
        set_error("holding register address out of range");
        return 0;
    }

    g_holding[address] = value;
    set_error("OK");
    return 1;
}

int __stdcall M485_GetHolding(unsigned short address)
{
    std::lock_guard<std::mutex> guard(g_lock);
    if (address >= RegisterCount)
    {
        set_error("holding register address out of range");
        return -1;
    }

    set_error("OK");
    return g_holding[address];
}

int __stdcall M485_SetInput(unsigned short address, unsigned short value)
{
    std::lock_guard<std::mutex> guard(g_lock);
    if (address >= RegisterCount)
    {
        set_error("input register address out of range");
        return 0;
    }

    g_input[address] = value;
    set_error("OK");
    return 1;
}

int __stdcall M485_GetInput(unsigned short address)
{
    std::lock_guard<std::mutex> guard(g_lock);
    if (address >= RegisterCount)
    {
        set_error("input register address out of range");
        return -1;
    }

    set_error("OK");
    return g_input[address];
}

int __stdcall M485_ProcessRtu(
    const unsigned char* request,
    int requestLength,
    unsigned char* response,
    int responseCapacity)
{
    std::lock_guard<std::mutex> guard(g_lock);

    if (request == nullptr || response == nullptr)
    {
        set_error("request and response buffers are required");
        return -1;
    }

    if (requestLength < 4)
    {
        set_error("request frame is too short");
        return -1;
    }

    if (request[0] != g_unitId)
    {
        set_error("request is for a different unit id");
        return 0;
    }

    if (!has_valid_crc(request, requestLength))
    {
        set_error("invalid request CRC");
        return -1;
    }

    if (requestLength < 8)
    {
        return exception_response(request[1], ExceptionIllegalDataValue, response, responseCapacity);
    }

    const auto functionCode = request[1];
    const auto address = read_be16(&request[2]);
    const auto argument = read_be16(&request[4]);

    switch (functionCode)
    {
    case 0x03:
        set_error("OK");
        return read_registers(g_holding, functionCode, address, argument, response, responseCapacity);
    case 0x04:
        set_error("OK");
        return read_registers(g_input, functionCode, address, argument, response, responseCapacity);
    case 0x06:
        if (address >= RegisterCount)
        {
            return exception_response(functionCode, ExceptionIllegalDataAddress, response, responseCapacity);
        }

        g_holding[address] = argument;
        if (responseCapacity < requestLength)
        {
            set_error("response buffer too small for write echo");
            return -1;
        }

        std::memcpy(response, request, static_cast<std::size_t>(requestLength));
        set_error("OK");
        return requestLength;
    default:
        return exception_response(functionCode, ExceptionIllegalFunction, response, responseCapacity);
    }
}

const char* __stdcall M485_LastError(void)
{
    return g_lastError;
}
