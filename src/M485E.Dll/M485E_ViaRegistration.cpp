#define _BUILDNODELAYERDLL
#include <VIA_CDLL.h>

/**
 * @brief Registers this DLL with the CAPL program instance.
 * @param service Vector service for the CAPL program that called
 *        registerCAPLDLL().
 * @note M485E does not retain the service pointer because it never calls back
 *       into CAPL. The CAPL handle is supplied separately to every M485E API.
 * @warning The undecorated x86 export name is provided by M485E.def as required
 *          by the Vector CAPL DLL loader.
 */
VIACLIENT(void) VIARegisterCDLL(VIACapl* service)
{
  if (service == nullptr) return;

  uint32 handle = 0U;
  const VIAResult result = service->GetCaplHandle(&handle);
  if (result != kVIA_OK) return;

  // Vector owns the handle domain, and zero is a valid handle for a CAPL program
  // instance. registerCAPLDLL() returns this same value to CAPL; the Core then
  // passes it to InitializeContext, so no duplicate service-to-handle map is needed.
}
