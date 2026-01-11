#pragma once
#include <windows.h>
#include <msctf.h>
#include <string>
#include <vector>
#include <strsafe.h>

// Global HINSTANCE for the DLL
extern HINSTANCE g_hInst;

// CLSID for the Text Service (IME)
// {E40A5D80-9999-4B5C-8F6D-3D4E5F6A7B8C}
static const GUID CLSID_UTIME = 
{ 0xe40a5d80, 0x9999, 0x4b5c, { 0x8f, 0x6d, 0x3d, 0x4e, 0x5f, 0x6a, 0x7b, 0x8c } };

// GUID for the Language Profile
// {E40A5D80-9999-4B5C-8F6D-3D4E5F6A7B8D}
static const GUID GUID_PROFILE = 
{ 0xe40a5d80, 0x9999, 0x4b5c, { 0x8f, 0x6d, 0x3d, 0x4e, 0x5f, 0x6a, 0x7b, 0x8d } };

// Utility to increment/decrement ref count for DLL server
void DllAddRef();
void DllRelease();

// Debug Logger Helper
void DebugLog(const wchar_t* format, ...);
