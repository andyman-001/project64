#include "..\Plugin.h"
#include <windows.h>

void FixUPXIssue ( BYTE * ProgramLocation );

CControl_Plugin::CControl_Plugin ( const char * FileName) {
	//Make sure all parts of the class are initialized
	m_Initilized           = false;
	m_RomOpen              = false;
	m_AllocatedControllers = false;
	hDll                   = NULL;
	_Notify                = NULL;
	m_Controllers[0]       = NULL;
	m_Controllers[1]       = NULL;
	m_Controllers[2]       = NULL;
	m_Controllers[3]       = NULL;
	UnloadPlugin();

	//Try to load the DLL library
	UINT LastErrorMode = SetErrorMode( SEM_FAILCRITICALERRORS );
	hDll = LoadLibrary(FileName);
	SetErrorMode(LastErrorMode);
	
	if (hDll == NULL) { 
		UnloadPlugin();
		return;
	}
	FixUPXIssue((BYTE *)hDll);

	//Get DLL information
	void (__cdecl *GetDllInfo) ( PLUGIN_INFO * PluginInfo );
	GetDllInfo = (void (__cdecl *)(PLUGIN_INFO *))GetProcAddress( (HMODULE)hDll, "GetDllInfo" );
	if (GetDllInfo == NULL) { UnloadPlugin(); return; }

	GetDllInfo(&m_PluginInfo);
	if (!ValidPluginVersion(&m_PluginInfo)) { UnloadPlugin(); return; }

	//Find entries for functions in DLL
	void  (__cdecl *InitFunc)     ( void );
	Config            = (void (__cdecl *)(DWORD))GetProcAddress( (HMODULE)hDll, "DllConfig" );
	ControllerCommand = (void (__cdecl *)(int,BYTE *))GetProcAddress( (HMODULE)hDll, "ControllerCommand" );
	GetKeys           = (void (__cdecl *)(int,BUTTONS*)) GetProcAddress( (HMODULE)hDll, "GetKeys" );
	ReadController    = (void (__cdecl *)(int,BYTE *))GetProcAddress( (HMODULE)hDll, "ReadController" );
	WM_KeyDown        = (void (__cdecl *)(DWORD,DWORD))GetProcAddress( (HMODULE)hDll, "WM_KeyDown" );
	WM_KeyUp          = (void (__cdecl *)(DWORD,DWORD))GetProcAddress( (HMODULE)hDll, "WM_KeyUp" );
	InitFunc          = (void (__cdecl *)(void)) GetProcAddress( (HMODULE)hDll, "InitiateControllers" );
	RomOpen           = (void (__cdecl *)(void)) GetProcAddress( (HMODULE)hDll, "RomOpen" );
	RomClosed         = (void (__cdecl *)(void)) GetProcAddress( (HMODULE)hDll, "RomClosed" );
	CloseDLL          = (void (__cdecl *)(void)) GetProcAddress( (HMODULE)hDll, "CloseDLL" );
	RumbleCommand     = (void (__cdecl *)(int, BOOL))GetProcAddress( (HMODULE)hDll, "RumbleCommand" );

	//version 101 functions
	SetSettingInfo   = (void (__cdecl *)(PLUGIN_SETTINGS *))GetProcAddress( (HMODULE)hDll, "SetSettingInfo" );
	PluginOpened     = (void (__cdecl *)(void))GetProcAddress( (HMODULE)hDll, "PluginLoaded" );

	//Make sure dll had all needed functions
	if (InitFunc       == NULL) { UnloadPlugin(); return;  }
	if (CloseDLL       == NULL) { UnloadPlugin(); return;  }

	if (m_PluginInfo.Version >= 0x0102)
	{
		if (SetSettingInfo  == NULL) { UnloadPlugin(); return; }
		if (PluginOpened    == NULL) { UnloadPlugin(); return; }

		PLUGIN_SETTINGS info;
		info.dwSize = sizeof(PLUGIN_SETTINGS);
		info.DefaultStartRange = FirstCtrlDefaultSet;
		info.SettingStartRange = FirstCtrlSettings;
		info.MaximumSettings   = MaxPluginSetting;
		info.NoDefault         = No_Default;
		info.DefaultLocation   = _Settings->LoadDword(UseSettingFromRegistry) ? SettingLocation_Registry : SettingLocation_CfgFile;
		info.handle            = _Settings;
		info.RegisterSetting   = (void (*)(void *,int,int,SettingDataType,SettingLocation,const char *,const char *, DWORD))CSettings::RegisterSetting;
		info.GetSetting        = (unsigned int (*)( void * handle, int ID ))CSettings::GetSetting;
		info.GetSettingSz      = (const char * (*)( void *, int, char *, int ))CSettings::GetSettingSz;
		info.SetSetting        = (void (*)(void *,int,unsigned int))CSettings::SetSetting;
		info.SetSettingSz      = (void (*)(void *,int,const char *))CSettings::SetSettingSz;
		info.UseUnregisteredSetting = NULL;

		SetSettingInfo(&info);
//		_Settings->UnknownSetting_CTRL = info.UseUnregisteredSetting;

		PluginOpened();
	}

	//Allocate our own controller
	m_AllocatedControllers = true;
	m_Controllers[0] = new CCONTROL(m_PluginControllers[0].Present,m_PluginControllers[0].RawData,m_PluginControllers[0].Plugin);
	m_Controllers[1] = new CCONTROL(m_PluginControllers[1].Present,m_PluginControllers[1].RawData,m_PluginControllers[1].Plugin);
	m_Controllers[2] = new CCONTROL(m_PluginControllers[2].Present,m_PluginControllers[2].RawData,m_PluginControllers[2].Plugin);
	m_Controllers[3] = new CCONTROL(m_PluginControllers[3].Present,m_PluginControllers[3].RawData,m_PluginControllers[3].Plugin);
}

CControl_Plugin::~CControl_Plugin (void) {
	Close();
	UnloadPlugin();
}

bool CControl_Plugin::Initiate ( CN64System * System, CMainGui * RenderWindow ) {
	_Notify =  RenderWindow->GetNotifyClass();
	m_PluginControllers[0].Present = FALSE;
	m_PluginControllers[0].RawData = FALSE;
	m_PluginControllers[0].Plugin  = PLUGIN_NONE;

	m_PluginControllers[1].Present = FALSE;
	m_PluginControllers[1].RawData = FALSE;
	m_PluginControllers[1].Plugin  = PLUGIN_NONE;

	m_PluginControllers[2].Present = FALSE;
	m_PluginControllers[2].RawData = FALSE;
	m_PluginControllers[2].Plugin  = PLUGIN_NONE;

	m_PluginControllers[3].Present = FALSE;
	m_PluginControllers[3].RawData = FALSE;
	m_PluginControllers[3].Plugin  = PLUGIN_NONE;

	//Get DLL information
	void (__cdecl *GetDllInfo) ( PLUGIN_INFO * PluginInfo );
	GetDllInfo = (void (__cdecl *)(PLUGIN_INFO *))GetProcAddress( (HMODULE)hDll, "GetDllInfo" );
	if (GetDllInfo == NULL) { return false; }

	PLUGIN_INFO PluginInfo;
	GetDllInfo(&PluginInfo);

	int val = sizeof(CONTROL);
	//Test Plugin version
	if (PluginInfo.Version == 0x0100) {
		//Get Function from DLL
		void (__cdecl *InitiateControllers_1_0)( HWND hMainWindow, CONTROL Controls[4] );
		InitiateControllers_1_0 = (void (__cdecl *)(HWND, CONTROL *))GetProcAddress( (HMODULE)hDll, "InitiateControllers" );
		if (InitiateControllers_1_0 == NULL) { return false; }
		InitiateControllers_1_0((HWND)RenderWindow->m_hMainWindow,m_PluginControllers);
		m_Initilized = true;
	}
	if (PluginInfo.Version >= 0x0101) {
		typedef struct {
			HWND hMainWindow;
			HINSTANCE hinst;

			BOOL MemoryBswaped;		// If this is set to TRUE, then the memory has been pre
									//   bswap on a dword (32 bits) boundry, only effects header. 
									//	eg. the first 8 bytes are stored like this:
									//        4 3 2 1   8 7 6 5
			BYTE * HEADER;			// This is the rom header (first 40h bytes of the rom)
			CONTROL *Controls;		// A pointer to an array of 4 controllers .. eg:
									// CONTROL Controls[4];
		} CONTROL_INFO;

		//Get Function from DLL		
		void (__cdecl *InitiateControllers_1_1)( CONTROL_INFO * ControlInfo );
		InitiateControllers_1_1 = (void (__cdecl *)(CONTROL_INFO *))GetProcAddress( (HMODULE)hDll, "InitiateControllers" );
		if (InitiateControllers_1_1 == NULL) { return false; }
		
		CONTROL_INFO ControlInfo;
		if (System == NULL) {
			BYTE Buffer[100];
			
			ControlInfo.Controls      = m_PluginControllers;
			ControlInfo.HEADER        = Buffer;
			ControlInfo.hinst         = GetModuleHandle(NULL);
			ControlInfo.hMainWindow   = (HWND)RenderWindow->m_hMainWindow;
			ControlInfo.MemoryBswaped = TRUE;
			InitiateControllers_1_1(&ControlInfo);
			m_Initilized = true;
		} else {
			ControlInfo.Controls      = m_PluginControllers;
			ControlInfo.HEADER        = System->_MMU->ROM;
			ControlInfo.hinst         = GetModuleHandle(NULL);
			ControlInfo.hMainWindow   = (HWND)RenderWindow->m_hMainWindow;
			ControlInfo.MemoryBswaped = TRUE;
			InitiateControllers_1_1(&ControlInfo);
			m_Initilized = true;
		}
	}

	//jabo had a bug so I call CreateThread so his dllmain gets called again
	DWORD ThreadID;
	HANDLE hthread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)DummyFunction,NULL,0, &ThreadID);	
	CloseHandle(hthread);

	
	return m_Initilized;
}

void CControl_Plugin::RomOpened ( void )
{
	//Real system ... then make the file as open
	if (!m_RomOpen) 
	{
		RomOpen();
		m_RomOpen = true;
	}
}

void CControl_Plugin::Close(void) {
	if (m_RomOpen) {
		RomClosed();
		m_RomOpen = false;
	}
	if (m_Initilized) {
		CloseDLL();
		m_Initilized = false;
	}
}

void CControl_Plugin::GameReset(void)
{
	if (m_RomOpen) 
	{
		RomClosed();
		RomOpen();
	}
}

bool CControl_Plugin::ValidPluginVersion(PLUGIN_INFO * PluginInfo) {
	switch (PluginInfo->Type) {
	case PLUGIN_TYPE_CONTROLLER:
		if (PluginInfo->Version == 0x0100) { return TRUE; }
		if (PluginInfo->Version == 0x0101) { return TRUE; }
		if (PluginInfo->Version == 0x0102) { return TRUE; }
		break;
	}
	return FALSE;
}

void CControl_Plugin::UnloadPlugin(void) {
	if (m_AllocatedControllers) {
		for (int count = 0; count < sizeof(m_Controllers) / sizeof(m_Controllers[0]); count++) {
			delete m_Controllers[count];
			m_Controllers[count] = NULL;
		}
	}
	memset(&m_PluginInfo,0,sizeof(m_PluginInfo));
	if (hDll != NULL ) {
		FreeLibrary((HMODULE)hDll);
		hDll = NULL;
	}
	m_AllocatedControllers = false;
	m_Controllers[0]       = NULL;
	m_Controllers[1]       = NULL;
	m_Controllers[2]       = NULL;
	m_Controllers[3]       = NULL;
	Config                 = NULL;
	ControllerCommand      = NULL;
	GetKeys                = NULL;
	ReadController         = NULL;
	WM_KeyDown             = NULL;
	WM_KeyUp               = NULL;
	CloseDLL               = NULL;
	RomOpen                = NULL;
	RomClosed              = NULL;

}

void CControl_Plugin::UpdateKeys (void) {
	if (!m_AllocatedControllers) { return; }
	for (int cont = 0; cont < sizeof(m_Controllers) / sizeof(m_Controllers[0]); cont++) {
		if (!m_Controllers[cont]->m_Present) { continue; }
		if (!m_Controllers[cont]->m_RawData) { 
			GetKeys(cont,&m_Controllers[cont]->m_Buttons);
		} else {
			if (_Notify)
			{
				_Notify->BreakPoint(__FILE__,__LINE__); 
			} else {
				_asm int 3
			}
		}
	}
	if (ReadController) { ReadController(-1,NULL); }
}

void CControl_Plugin::SetControl(CControl_Plugin const * const Plugin) {
	if (m_AllocatedControllers) {
		for (int count = 0; count < sizeof(m_Controllers) / sizeof(m_Controllers[0]); count++) {
			delete m_Controllers[count];
			m_Controllers[count] = NULL;
		}
	}
	m_AllocatedControllers = false;
	for (int count = 0; count < sizeof(m_Controllers) / sizeof(m_Controllers[0]); count++) {
		m_Controllers[count] = Plugin->m_Controllers[count];
	}

}

CCONTROL::CCONTROL(DWORD &Present,DWORD &RawData, int &PlugType) :
	m_Present(Present),m_RawData(RawData),m_PlugType(PlugType)
{
	m_Buttons.Value = 0;
}