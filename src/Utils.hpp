#pragma once

#include <cstdio>
#include <print>

#include <windows.h>
#include <shobjidl.h>
#include <comdef.h>

namespace Utils
{
	inline void initConsole()
	{
		AllocConsole();
		FILE* f;

		freopen_s(&f, "CONOUT$", "w", stdout);
	}

	inline void setConsoleTitle(const std::string& title) { SetConsoleTitleA(title.data()); }

	inline bool selectFile(std::string& outPath)
	{
		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

		if (FAILED(hr))
			return false;

		IFileDialog* pFileDialog = nullptr;
		hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, (void**)&pFileDialog);

		if (FAILED(hr))
		{
			CoUninitialize();
			return false;
		}

		COMDLG_FILTERSPEC fileTypes[] = { { L"Executable Files", L"*.exe" }, { L"All Files", L"*.*" } };
		pFileDialog->SetFileTypes(2, fileTypes);
		pFileDialog->SetFileTypeIndex(1);

		hr = pFileDialog->Show(NULL);

		if (FAILED(hr))
		{
			pFileDialog->Release();
			CoUninitialize();
			return false;
		}

		IShellItem* pItem;
		hr = pFileDialog->GetResult(&pItem);

		if (FAILED(hr))
		{
			pItem->Release();
			pFileDialog->Release();
			CoUninitialize();
			return false;
		}

		PWSTR pszPath;
		hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);

		if (SUCCEEDED(hr))
		{
			int32_t size = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, nullptr, 0, nullptr, nullptr);
			outPath.resize(size - 1);
			WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, &outPath[0], size, nullptr, nullptr);

			CoTaskMemFree(pszPath);
		}

		pItem->Release();
		pFileDialog->Release();

		CoUninitialize();
		return SUCCEEDED(hr);
	}
}
