#include <Windows.h>
#include <stdio.h>
#include <winternl.h>
#include <strsafe.h>

#define SERVICE_NAME L"SilentLoad"
#define DRIVER_PATH L"\\??\\C:\\Windows\\System32\\drivers\\SilentLoad.sys"

typedef NTSTATUS(NTAPI* _NtLoadDriver)(PUNICODE_STRING DriverServiceName);

static _NtLoadDriver NtLoadDriver = NULL;

static bool GrantPrivilege(LPCTSTR Privilege)
{
	HANDLE Token;
	TOKEN_PRIVILEGES TokenPrivileges;
	LUID Luid;

	if (!LookupPrivilegeValueW(NULL, SE_LOAD_DRIVER_NAME, &Luid))
		return false;
	
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &Token))
		return false;

	TokenPrivileges = { 0 };
	TokenPrivileges.PrivilegeCount = 1;
	TokenPrivileges.Privileges[0].Luid = Luid;
	TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if (!AdjustTokenPrivileges(Token, FALSE, &TokenPrivileges, sizeof(TOKEN_PRIVILEGES), NULL, NULL) 
		|| GetLastError() == ERROR_NOT_ALL_ASSIGNED)
	{
		CloseHandle(Token);
		return false;
	}

	CloseHandle(Token);

	return true;
}

static _NtLoadDriver ResolveNtLoadDriver()
{
	HMODULE Handle;
	
	Handle = GetModuleHandleW(L"ntdll.dll");
	if (!Handle)
		return NULL;

	return (_NtLoadDriver)GetProcAddress(Handle, "NtLoadDriver");
}

static bool AddService(LPCWSTR ServiceName, LPCWSTR DriverPath)
{
	HKEY ServicesKey;
	DWORD ImagePathLength;
	DWORD ServiceType;
	DWORD ServiceStartType;
	DWORD ServiceErrorControl;
	DWORD ServiceNameLength;

	if (!NT_SUCCESS(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0, KEY_ALL_ACCESS, &ServicesKey)))
		goto Error;
	
	if (!NT_SUCCESS(RegCreateKeyW(ServicesKey, ServiceName, &ServicesKey)))
		goto Error;

	ImagePathLength = (wcslen(DriverPath) + 1) * sizeof(WCHAR);
	if (!NT_SUCCESS(RegSetValueExW(ServicesKey, L"ImagePath", 0, REG_EXPAND_SZ, (LPBYTE)DriverPath, ImagePathLength)))
		goto Error;

	ServiceType = SERVICE_KERNEL_DRIVER;
	if (!NT_SUCCESS(RegSetValueExW(ServicesKey, L"Type", 0, REG_DWORD, (LPBYTE)&ServiceType, sizeof(ServiceType))))
		goto Error;

	ServiceStartType = SERVICE_DEMAND_START;
	if (!NT_SUCCESS(RegSetValueExW(ServicesKey, L"Start", 0, REG_DWORD, (LPBYTE)&ServiceStartType, sizeof(ServiceStartType))))
		goto Error;

	ServiceErrorControl = SERVICE_ERROR_NORMAL;
	if (!NT_SUCCESS(RegSetValueExW(ServicesKey, L"ErrorControl", 0, REG_DWORD, (LPBYTE)&ServiceErrorControl, sizeof(ServiceErrorControl))))
		goto Error;

	ServiceNameLength = (wcslen(ServiceName) + 1) * sizeof(WCHAR);
	if (!NT_SUCCESS(RegSetValueExW(ServicesKey, L"DisplayName", 0, REG_SZ, (LPBYTE)ServiceName, ServiceNameLength)))
		goto Error;

	RegCloseKey(ServicesKey);

	return true;

Error:
	RegCloseKey(ServicesKey);

	return false;
}

static bool RemoveService(LPCWSTR ServiceName)
{
	WCHAR RegistryPath[MAX_PATH];

	StringCchPrintfW(RegistryPath, ARRAYSIZE(RegistryPath), L"SYSTEM\\CurrentControlSet\\Services\\%s", ServiceName);
	
	return NT_SUCCESS(RegDeleteTreeW(HKEY_LOCAL_MACHINE, RegistryPath));
}

static bool LoadDriver(LPCWSTR ServiceName)
{
	UNICODE_STRING DriverServiceName;
	WCHAR Buffer[MAX_PATH];
	NTSTATUS Status;

    StringCchPrintfW(Buffer, ARRAYSIZE(Buffer), L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\%s", ServiceName);
	RtlInitUnicodeString(&DriverServiceName, Buffer);

	Status = NtLoadDriver(&DriverServiceName);
	if (Status == 0xC000010E)
	{
		printf("Driver already loaded\n");
		return true;
	}

	return NT_SUCCESS(Status);

}

int main(int argc, char* argv[])
{
	if (!GrantPrivilege(SE_LOAD_DRIVER_NAME))
	{
		printf("Failed to grant privilege\n");
		goto End;
	}

	NtLoadDriver = ResolveNtLoadDriver();
	if (!NtLoadDriver)
	{
		printf("Failed to resolve NtLoadDriver\n");
		goto End;
	}

	printf("NtLoadDriver: 0x%p\n", NtLoadDriver);

	if (!AddService(SERVICE_NAME, DRIVER_PATH))
	{
		printf("Failed to add service\n");
		goto End;
	}

	if (!LoadDriver(SERVICE_NAME))
	{
		printf("Failed to load driver\n");
		goto End;
	}


End:
	if (!RemoveService(SERVICE_NAME))
		printf("Failed to remove service\n");

	return 0;
}