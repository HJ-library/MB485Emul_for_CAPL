#include <windows.h>

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
  // Runtime resources are created by InitializeContext, never under the Windows
  // loader lock. DllMain therefore remains intentionally side-effect free.
  return TRUE;
}
