// InfInstall.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <filesystem>

HRESULT InstallDriver(const TCHAR *HardwareId, const TCHAR *InfPath, DWORD offset)
{
	HRESULT res = 0;
	HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
	SP_DEVINSTALL_PARAMS InstallParams = { sizeof(SP_DEVINSTALL_PARAMS), 0 };
	SP_DRVINFO_DATA DriverInfoData = { sizeof(SP_DRVINFO_DATA), 0 };
	SP_DEVINFO_DATA DeviceInfoData;
	GUID ClassGUID;
	TCHAR ClassName[MAX_CLASS_NAME_LEN];
	TCHAR hwIdList[LINE_LEN + 4];

	//
	// List of hardware ID's must be double zero-terminated
	//
	ZeroMemory(hwIdList, sizeof(hwIdList));
	if (StringCchCopy(hwIdList, LINE_LEN, HardwareId) != S_OK)
	{
		goto final1;
	}

	//
	// Use the INF File to extract the Class GUID.
	//
	if (!SetupDiGetINFClass(InfPath, &ClassGUID, ClassName, sizeof(ClassName) / sizeof(ClassName[0]), 0))
	{
		goto final1;
	}

	//
	// Create the container for the to-be-created Device Information Element.
	//
	DeviceInfoSet = SetupDiCreateDeviceInfoList(&ClassGUID, 0);
	if (DeviceInfoSet == INVALID_HANDLE_VALUE)
	{
		goto final1;
	}

	//
	// Now create the element.
	// Use the Class GUID and Name from the INF file.
	//
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	if (!SetupDiCreateDeviceInfo(DeviceInfoSet,
		ClassName,
		&ClassGUID,
		NULL,
		0,
		DICD_GENERATE_ID,
		&DeviceInfoData))
	{
		goto final1;
	}

	//
	// Add the HardwareID to the Device's HardwareID property.
	//
	if (!SetupDiSetDeviceRegistryProperty(DeviceInfoSet,
		&DeviceInfoData,
		SPDRP_HARDWAREID,
		(LPBYTE)hwIdList,
		((DWORD)_tcslen(hwIdList) + 1 + 1) * sizeof(TCHAR)))
	{
		goto final1;
	}

	InstallParams.FlagsEx = DI_FLAGSEX_ALLOWEXCLUDEDDRVS | DI_FLAGSEX_ALWAYSWRITEIDS;
	InstallParams.Flags = DI_QUIETINSTALL | DI_ENUMSINGLEINF;
	_tcscpy_s(InstallParams.DriverPath, InfPath);

	if (!SetupDiSetDeviceInstallParams(DeviceInfoSet, &DeviceInfoData, &InstallParams)) {
		goto final1;
	}

	if (!SetupDiBuildDriverInfoList(DeviceInfoSet, &DeviceInfoData, SPDIT_CLASSDRIVER))
	{
		goto final1;
	}

	// Use first best driver (since specified by inf file)
	if (!SetupDiEnumDriverInfo(DeviceInfoSet, &DeviceInfoData, SPDIT_CLASSDRIVER, offset, &DriverInfoData))
	{
		goto final2;
	}

	if (!SetupDiSetSelectedDriver(DeviceInfoSet, &DeviceInfoData, &DriverInfoData))
	{
		goto final2;
	}

	//
	// Transform the registry element into an actual devnode
	// in the PnP HW tree.
	//
	if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE,
		DeviceInfoSet,
		&DeviceInfoData))
	{
		goto final2;
	}

	if (!DiInstallDevice(nullptr, DeviceInfoSet, &DeviceInfoData, &DriverInfoData, 0, nullptr))
	{
		// Ensure that the device entry in \ROOT\ENUM\ will be removed...
		SetupDiRemoveDevice(DeviceInfoSet, &DeviceInfoData);
		goto final2;
	}

final2:
	if (res == S_OK)
		res = HRESULT_FROM_WIN32(GetLastError());

	SetupDiDestroyDriverInfoList(DeviceInfoSet, &DeviceInfoData, SPDIT_CLASSDRIVER);

final1:
	if (res == S_OK)
		res = HRESULT_FROM_WIN32(GetLastError());

	if (DeviceInfoSet != INVALID_HANDLE_VALUE)
	{
		SetupDiDestroyDeviceInfoList(DeviceInfoSet);
	}

	return res;
}

int _tmain(int argc, TCHAR *argv[])
{
	if (argc < 3) {
		std::cout << "Usage: InfInstall HARDWAREID INFPATH [INFINDEX]" << std::endl;
		return 1;
	}
	TCHAR *hardwareID = argv[1];
	TCHAR *infPath = argv[2];
	DWORD index = 0;
	std::filesystem::path infFullPath;
	if (argc > 3) {
		index = DWORD(_tstoi64(argv[3]));
	}
	try {
		infFullPath = std::filesystem::canonical(infPath);
	} catch (std::filesystem::filesystem_error &e) {
		std::cout << "Cannot find specified inf " << e.what() << std::endl;
		return 1;
	}
	
	auto res = InstallDriver(hardwareID, infFullPath.native().data(), index);

	if (res != S_OK) {
		return 1;
	}

	return 0;
}
