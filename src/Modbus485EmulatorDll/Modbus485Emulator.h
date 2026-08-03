#pragma once

#ifdef M485_EXPORTS
#define M485_API extern "C" __declspec(dllexport)
#else
#define M485_API extern "C" __declspec(dllimport)
#endif

M485_API int __stdcall M485_Init(unsigned char unitId);
M485_API int __stdcall M485_Reset(void);
M485_API int __stdcall M485_SetHolding(unsigned short address, unsigned short value);
M485_API int __stdcall M485_GetHolding(unsigned short address);
M485_API int __stdcall M485_SetInput(unsigned short address, unsigned short value);
M485_API int __stdcall M485_GetInput(unsigned short address);
M485_API int __stdcall M485_ProcessRtu(
    const unsigned char* request,
    int requestLength,
    unsigned char* response,
    int responseCapacity);
M485_API const char* __stdcall M485_LastError(void);
