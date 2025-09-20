#pragma once

// Function type macros.
#define VARARGS     __cdecl											/* Functions with variable arguments */
#define STDCALL		__stdcall										/* Standard calling convention */

#define INLINE inline                                               /* Inline */


// DLL export and import definitions
#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)


