#include "fcsdk_test.h"
#include "Logger.h"

#ifdef VS2013
//If you use cpprest(casablanca) do not forget to add "_NO_ASYNCRTIMP" to your C++ Preprosessor setting!
//https://github.com/microsoft/cpprestsdk/issues/344
#include "cpprest/http_client.h"
#include "cpprest/filestream.h"
#include "cpprest/json.h"
// end of cpprest
#else
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "json/json.h"
#include "json/writer.h"
#include "json/reader.h"
#include "httplib.h"

#endif

//combo data
TCHAR* resolutions[] = {
	TEXT("RESOLUTION_AUTO"),
	TEXT("RESOLUTION_160x120"),
	TEXT("RESOLUTION_176x144"),
	TEXT("RESOLUTION_320x180"),
	TEXT("RESOLUTION_320x240"),
	TEXT("RESOLUTION_352x288"),
	TEXT("RESOLUTION_640x360"),
	TEXT("RESOLUTION_640x480"),
	TEXT("RESOLUTION_1280x720"),
	TEXT("RESOLUTION_1920x1080"),
	0 };

TCHAR* fps_values[] = {
	TEXT("FPS_5"),
	TEXT("FPS_10"),
	TEXT("FPS_15"),
	TEXT("FPS_20"),
	TEXT("FPS_25"),
	TEXT("FPS_30"),
	TEXT("FPS_35"),
	TEXT("FPS_40"),
	0 };

TCHAR* videotypes[] = {
	TEXT("AUTO"),
	TEXT("I420"),
	TEXT("YV12"),
	TEXT("YUY2"),
	TEXT("UYVY"),
	TEXT("MJPG"),
	TEXT("ARGB"),
	TEXT("RGB24"),
	0 };

//
// utils
//

//https://stackoverflow.com/questions/215963/how-do-you-properly-use-widechartomultibyte/3999597#3999597
// convert string to wstring
std::wstring s2ws(const std::string& str)
{
	if (str.empty()) return std::wstring();
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

// convert wstring to string
std::string ws2s(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

// read a string of the control
std::string GetWindowStringText(HWND hwnd)
{
	int len = GetWindowTextLength(hwnd) + 1;
	std::vector<wchar_t> buf(len);
	GetWindowText(hwnd, &buf[0], len);
	std::wstring wide = &buf[0];
	std::string s(wide.begin(), wide.end());
	return s;
}

// write a string to the control
void SetWindowStringText(HWND hwnd, std::string message)
{
	std::wstring wstr = s2ws(message);
	LPCWSTR pwstr = wstr.c_str();
	SetWindowText(hwnd, pwstr);
}

//reading .ini file
std::string GetConfigString(const std::string& filePath, const char* pSectionName, const char* pKeyName)
{
	if (filePath.empty()) {
		return "";
	}
	char buf[MAX_PATH];
	GetPrivateProfileStringA(pSectionName, pKeyName, "", buf, sizeof(buf), filePath.c_str());
	std::ostringstream oss;
	oss << buf;
	return oss.str();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////

//
// constractor
//

FcsdkTest::FcsdkTest(int nCmdLn)
{
	sessionStr = "";
	mCall = NULL;

	WNDCLASSEX w;
	ZeroMemory(&w, sizeof(w));
	w.cbSize = sizeof(w);
	w.hCursor = LoadCursor(NULL, IDC_ARROW);
	w.hbrBackground = HBRUSH(COLOR_WINDOW + 1);
	w.lpfnWndProc = MessageRouter;
	w.lpszClassName = TEXT("FCSDKTEST");
	w.style = CS_VREDRAW | CS_HREDRAW;
	isRinging = false;
	isAutoAnswer = false;
	form_width = 0;
	form_height = 0;

	app_camera_resolution_idx = 0;
	app_camera_fps_idx = 0;
	app_camera_videotype_idx = 0;
	app_camera_videodevice_idx = 0;

	std::string filePath = ".\\settings.ini";
	HOST_URL = GetConfigString(filePath, "FcsdkTest", "HostUrl");
	USERNAME = GetConfigString(filePath, "FcsdkTest", "Username");
	PASSWORD = GetConfigString(filePath, "FcsdkTest", "Password");
	DIALNUM  = GetConfigString(filePath, "FcsdkTest", "Dialnum");

	RegisterClassEx(&w);

	form_width  = 800;
	form_height = 600;

	windowHandle = CreateWindowEx(
		NULL, 
		TEXT("FCSDKTEST"), 
		TEXT("FCSDK test"), 
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
		CW_USEDEFAULT, 
		CW_USEDEFAULT, 
		form_width, 
		form_height, 
		NULL, NULL, NULL, this);
	if (windowHandle != INVALID_HANDLE_VALUE)
	{
		ShowWindow(windowHandle, nCmdLn);
		UpdateWindow(windowHandle);
	}
}

//
// Windows Message Loop
//

int FcsdkTest::Run()
{
	MSG msg{ 0 };
	if (windowHandle != INVALID_HANDLE_VALUE)
	{
		while (GetMessage(&msg, 0, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return msg.wParam;
}

//
// "Static" Message Router
// lpfnWndProc can call only static member functions.
// CreateWindow() will call this first. And this MessageRouter will call the user's WndProc (main)
// https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
//
LRESULT CALLBACK FcsdkTest::MessageRouter(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

	FcsdkTest* app;
	//If the window has been created we recover the class pointer from the window extra data space
	app = (FcsdkTest*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	if (!app){
		if (msg == WM_NCCREATE)
		{
			//When the window is created, lParam contains a pointer to the current class instance. We cast it and asign it to a pointer to the class
			//That way we can access its members from a static context (as MessageRouter's)
			app = (FcsdkTest*)(((LPCREATESTRUCT)lParam)->lpCreateParams);
			if (app){
				//We store the reteived pointer to the window extra data space for it to be available later
				SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)app);
				//We call the actual WndProc. This one is a member of the class and has access to its members
				return app->WndProc(hWnd, msg, wParam, lParam);
			}
		}
	}
	else {
		//We call the actual WndProc. This one is a member of the class and has access to its members
		return app->WndProc(hWnd, msg, wParam, lParam);
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

//
// My Dynamic Window Procedure
//
LRESULT CALLBACK FcsdkTest::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int num = 0;
	int wmId = 0;
	int Notify = 0;
	std::string status = "";

	switch (msg)
	{
		case WM_COMMAND:
			//****************
			wmId = LOWORD(wParam);
			Notify = HIWORD(wParam);
			switch (wmId)
			{
			case BUTTON_LOGIN:
				Login_with_https();
				break;

			case BUTTON_CALL:
				Call_Start();
				break;

			case BUTTON_HANGUP:
				Call_End();
				break;

			case BUTTON_HOLD:
				Call_Hold();
				break;

			case BUTTON_UNHOLD:
				Call_Unhold();
				break;

			case RADIO_A_SENDRECV:
				if (Notify == BN_CLICKED) {
					mAudioDirection = acb::MediaDirection::SEND_AND_RECEIVE;
					//MessageBox(windowHandle, L"mAudioDirection = acb::MediaDirection::SEND_AND_RECEIVE", TEXT("Information"), 0);
				}
				break;
			case RADIO_A_SENDONLY:
				if (Notify == BN_CLICKED) {
					mAudioDirection = acb::MediaDirection::SEND_ONLY; 
					//MessageBox(windowHandle, L"mAudioDirection = acb::MediaDirection::SEND_ONLY", TEXT("Information"), 0);
				}
				break;
			case RADIO_A_RECVONLY:
				if (Notify == BN_CLICKED) {
					mAudioDirection = acb::MediaDirection::RECEIVE_ONLY; 
					//MessageBox(windowHandle, L"mAudioDirection = acb::MediaDirection::RECEIVE_ONLY", TEXT("Information"), 0);
				}
				break;
			case RADIO_A_NONE:
				if (Notify == BN_CLICKED) {
					mAudioDirection = acb::MediaDirection::NONE; 
					//MessageBox(windowHandle, L"mAudioDirection = acb::MediaDirection::NONE", TEXT("Information"), 0);
				}
				break;

			case RADIO_V_SENDRECV:
				if (Notify == BN_CLICKED) {
					mVideoDirection = acb::MediaDirection::SEND_AND_RECEIVE; 
					//MessageBox(windowHandle, L"mVideoDirection = acb::MediaDirection::SEND_AND_RECEIVE", TEXT("Information"), 0);
				}
				break;
			case RADIO_V_SENDONLY:
				if (Notify == BN_CLICKED) {
					mVideoDirection = acb::MediaDirection::SEND_ONLY; 
					//MessageBox(windowHandle, L"mVideoDirection = acb::MediaDirection::SEND_ONLY", TEXT("Information"), 0);
				}
				break;
			case RADIO_V_RECVONLY:
				if (Notify == BN_CLICKED) {
					mVideoDirection = acb::MediaDirection::RECEIVE_ONLY; 
					//MessageBox(windowHandle, L"mVideoDirection = acb::MediaDirection::RECEIVE_ONLY", TEXT("Information"), 0);
				}
				break;
			case RADIO_V_NONE:
				if (Notify == BN_CLICKED) {
					mVideoDirection = acb::MediaDirection::NONE; 
					//MessageBox(windowHandle, L"mVideoDirection = acb::MediaDirection::NONE", TEXT("Information"), 0);
				}
				break;

			case BUTTON_V_MUTE:
				if (BST_CHECKED == SendMessage(checkbox_video_mute_, BM_GETCHECK, 0, 0)){
					Call_Video_Enable(false);
				}
				else{
					Call_Video_Enable(true);
				}
				break;

			case BUTTON_A_MUTE:
				if (BST_CHECKED == SendMessage(checkbox_audio_mute_, BM_GETCHECK, 0, 0)){
					Call_Audio_Enable(false);
				}
				else{
					Call_Audio_Enable(true);
				}
				break;

			case BUTTON_AUTO_ANS:
				if (BST_CHECKED == SendMessage(checkbox_auto_answer_, BM_GETCHECK, 0, 0)) {
					isAutoAnswer = true;
				}
				else {
					isAutoAnswer = false;
				}
				break;

			case BUTTON_DTMF_0:
				SendDTMF("0");
				break;
			case BUTTON_DTMF_1:
				SendDTMF("1");
				break;
			case BUTTON_DTMF_2:
				SendDTMF("2");
				break;
			case BUTTON_DTMF_3:
				SendDTMF("3");
				break;
			case BUTTON_DTMF_4:
				SendDTMF("4");
				break;
			case BUTTON_DTMF_5:
				SendDTMF("5");
				break;
			case BUTTON_DTMF_6:
				SendDTMF("6");
				break;
			case BUTTON_DTMF_7:
				SendDTMF("7");
				break;
			case BUTTON_DTMF_8:
				SendDTMF("8");
				break;
			case BUTTON_DTMF_9:
				SendDTMF("9");
				break;
			case BUTTON_DTMF_AST:
				SendDTMF("*");
				break;
			case BUTTON_DTMF_PND:
				SendDTMF("#");
				break;
			case BUTTON_DTMF_A:
				SendDTMF("A");
				break;
			case BUTTON_DTMF_B:
				SendDTMF("B");
				break;
			case BUTTON_DTMF_C:
				SendDTMF("C");
				break;
			case BUTTON_DTMF_D:
				SendDTMF("D");
				break;
			case COMBO_RESOLUTION:
				if (Notify == CBN_SELCHANGE) {
					num = (int)SendMessage(combo_resolution_, CB_GETCURSEL, 0, 0);
					app_camera_resolution_idx = num;
					if (mUC != nullptr) {
						SetResolution(app_camera_resolution_idx);
					}
				}
				break;
			case COMBO_FPS:
				if (Notify == CBN_SELCHANGE) {
					num = (int)SendMessage(combo_fps_, CB_GETCURSEL, 0, 0);
					app_camera_fps_idx = num;
					if (mUC != nullptr) {
						SetFps(app_camera_fps_idx);
					}
				}
				break;
			case COMBO_VIDEOTYPE:
				if (Notify == CBN_SELCHANGE) {
					num = (int)SendMessage(combo_videotype_, CB_GETCURSEL, 0, 0);
					app_camera_videotype_idx = num;
					if (mUC != nullptr) {
						SetVideotype(app_camera_videotype_idx);
					}
				}
				break;
			case COMBO_DEVICE:
				if (Notify == CBN_SELCHANGE) {
					num = (int)SendMessage(combo_videodevice_, CB_GETCURSEL, 0, 0);
					app_camera_videodevice_idx = num;
					if (mUC != nullptr) {
						SetVideoDevice(app_camera_videodevice_idx);
					}
				}
				break;
			}//**************** end of the "case WM_COMMAND:"
			break;

		case WM_CREATE:
			CreateUI(hWnd);
			break;
		case WM_CLOSE:
			status = GetWindowStringText(static_status_);
			if (status == "IN_CALL") {
				Call_End();
				Sleep(2);
			}
			DestroyWindow(hWnd);
			break;

		case WM_DESTROY:
			CloseWindow(hWnd);
			PostQuitMessage(0);
			break;
		case WM_SIZE:
			form_width = LOWORD(lParam);
			form_height = HIWORD(lParam);
			ResizeUI(hWnd, form_width, form_height);
			break;
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
			break;
	}
	return 0;
}

//
// App code
//
void FcsdkTest::SetResolution(int num)
{
	if (mUC == nullptr || mUC->GetPhone() == nullptr) {
		return;
	}
	switch (num) {
		case 0:
			mUC->GetPhone()->SetPreferredCaptureResolution(acb::VideoCaptureResolution::RESOLUTION_AUTO);
			break;
		case 1:
			mUC->GetPhone()->SetPreferredCaptureResolution(acb::VideoCaptureResolution::RESOLUTION_160x120);
			break;
		case 2:
			mUC->GetPhone()->SetPreferredCaptureResolution(acb::VideoCaptureResolution::RESOLUTION_176x144);
			break;
		case 3:
			mUC->GetPhone()->SetPreferredCaptureResolution(acb::VideoCaptureResolution::RESOLUTION_320x180);
			break;
		case 4:
			mUC->GetPhone()->SetPreferredCaptureResolution(acb::VideoCaptureResolution::RESOLUTION_320x240);
			break;
		case 5:
			mUC->GetPhone()->SetPreferredCaptureResolution(acb::VideoCaptureResolution::RESOLUTION_352x288);
			break;
		case 6:
			mUC->GetPhone()->SetPreferredCaptureResolution(acb::VideoCaptureResolution::RESOLUTION_640x360);
			break;
		case 7:
			mUC->GetPhone()->SetPreferredCaptureResolution(acb::VideoCaptureResolution::RESOLUTION_640x480);
			break;
		case 8:
			mUC->GetPhone()->SetPreferredCaptureResolution(acb::VideoCaptureResolution::RESOLUTION_1280x720);
			break;
		case 9:
			mUC->GetPhone()->SetPreferredCaptureResolution(acb::VideoCaptureResolution::RESOLUTION_1920x1080);
			break;
	}

}
void FcsdkTest::SetFps(int num)
{
	if (mUC == nullptr || mUC->GetPhone() == nullptr) {
		return;
	}
	switch (num) {
	case 0:
		mUC->GetPhone()->SetPreferredCaptureFrameRate(5);
		break;
	case 1:
		mUC->GetPhone()->SetPreferredCaptureFrameRate(10);
		break;
	case 2:
		mUC->GetPhone()->SetPreferredCaptureFrameRate(15);
		break;
	case 3:
		mUC->GetPhone()->SetPreferredCaptureFrameRate(20);
		break;
	case 4:
		mUC->GetPhone()->SetPreferredCaptureFrameRate(25);
		break;
	case 5:
		mUC->GetPhone()->SetPreferredCaptureFrameRate(30);
		break;
	case 6:
		mUC->GetPhone()->SetPreferredCaptureFrameRate(35);
		break;
	case 7:
		mUC->GetPhone()->SetPreferredCaptureFrameRate(40);
		break;
	}
}
void FcsdkTest::SetVideotype(int num)
{
	if (mUC == nullptr || mUC->GetPhone() == nullptr) {
		return;
	}
#ifndef VS2013
	switch (num) {
	case 0:
		mUC->GetPhone()->SetPreferredVideoType(acb::VideoType::AUTO);
		break;
	case 1:
		mUC->GetPhone()->SetPreferredVideoType(acb::VideoType::I420);
		break;
	case 2:
		mUC->GetPhone()->SetPreferredVideoType(acb::VideoType::YV12);
		break;
	case 3:
		mUC->GetPhone()->SetPreferredVideoType(acb::VideoType::YUY2);
		break;
	case 4:
		mUC->GetPhone()->SetPreferredVideoType(acb::VideoType::UYVY);
		break;
	case 5:
		mUC->GetPhone()->SetPreferredVideoType(acb::VideoType::MJPG);
		break;
	case 6:
		mUC->GetPhone()->SetPreferredVideoType(acb::VideoType::ARGB);
		break;
	case 7:
		mUC->GetPhone()->SetPreferredVideoType(acb::VideoType::RGB24);
		break;
	}
#endif
}

void FcsdkTest::SetVideoDevice(int num)
{
	if (mUC == nullptr || mUC->GetPhone() == nullptr) {
		return;
	}
#ifndef VS2013
	if (num == 0) {
		mUC->GetPhone()->SetVideoDeviceUniqueName("");
	}
	std::string devname = mUC->GetPhone()->GetVideoDeviceUniqueName(num - 1);
	mUC->GetPhone()->SetVideoDeviceUniqueName(devname);
#endif
}

void FcsdkTest::CreateUI(HWND hWnd)
{
	//
	// login UI
	//
	std::wstring wss_hosturl;
	wss_hosturl  = s2ws(HOST_URL);
	std::wstring wss_username;
	wss_username = s2ws(USERNAME);
	std::wstring wss_password;
	wss_password = s2ws(PASSWORD);
	std::wstring wss_dialnum;
	wss_dialnum  = s2ws(DIALNUM);

	edit_host_ =
		CreateWindow(TEXT("EDIT"), wss_hosturl.c_str(),
			WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE,
			10, 20, 343, 28, hWnd, (HMENU)EDIT_HOST, GetModuleHandle(NULL), NULL);

	edit_port_ =
		CreateWindow(TEXT("EDIT"), TEXT("443"),
			WS_CHILD | ES_LEFT | WS_BORDER | ES_NUMBER | WS_VISIBLE,
			355, 20, 80, 28, hWnd, (HMENU)EDIT_PORT, GetModuleHandle(NULL), NULL);

	edit_username_ =
		CreateWindow(TEXT("EDIT"), wss_username.c_str(),
			WS_CHILD | ES_LEFT | WS_BORDER | ES_NUMBER | WS_VISIBLE,
			450, 20, 80, 28, hWnd, (HMENU)EDIT_USERNAME, GetModuleHandle(NULL), NULL);

	edit_password_ =
		CreateWindow(TEXT("EDIT"), wss_password.c_str(),
			WS_CHILD | ES_LEFT | WS_BORDER | ES_PASSWORD | WS_VISIBLE,
			535, 20, 80, 28, hWnd, (HMENU)EDIT_PASSWORD, GetModuleHandle(NULL), NULL);

	CreateWindow(TEXT("BUTTON"), TEXT("LOGIN"),
		WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE,
		633, 20, 95, 28, hWnd, (HMENU)BUTTON_LOGIN, GetModuleHandle(NULL), NULL);

	static_session_ =
		CreateWindow(TEXT("STATIC"), TEXT("Session Info:"),
			WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE,
			10, 83, 620, 20, hWnd, (HMENU)STATIC_SESSION, GetModuleHandle(NULL), NULL);

	//Radio buttons
	CreateWindow(TEXT("STATIC"), TEXT("Video:"),
		WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE,
		10, 110, 100, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);

	radio_v_sendrecv_ = 
	CreateWindow(TEXT("BUTTON"), TEXT("SendRecv"), WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,	115, 110, 100, 20, hWnd, (HMENU)RADIO_V_SENDRECV, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("SendOnly"), WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE | BS_AUTORADIOBUTTON, 225, 110, 100, 20, hWnd, (HMENU)RADIO_V_SENDONLY, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("RecvOnly"), WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE | BS_AUTORADIOBUTTON, 335, 110, 100, 20, hWnd, (HMENU)RADIO_V_RECVONLY, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("None"),     WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE | BS_AUTORADIOBUTTON, 445, 110, 100, 20, hWnd, (HMENU)RADIO_V_NONE, GetModuleHandle(NULL), NULL);

	checkbox_video_mute_ = 
	CreateWindow(TEXT("BUTTON"), TEXT("Video Mute"), WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE | BS_AUTOCHECKBOX, 555, 110, 100, 20, hWnd, (HMENU)BUTTON_V_MUTE, GetModuleHandle(NULL), NULL);


	CreateWindow(TEXT("STATIC"), TEXT("Audio:"),
		WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE,
		10, 135, 100, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);

	radio_a_sendrecv_ = 
	CreateWindow(TEXT("BUTTON"), TEXT("SendRecv"), WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,	115, 135, 100, 20, hWnd, (HMENU)RADIO_A_SENDRECV, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("SendOnly"), WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE | BS_AUTORADIOBUTTON, 225, 135, 100, 20, hWnd, (HMENU)RADIO_A_SENDONLY, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("RecvOnly"), WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE | BS_AUTORADIOBUTTON, 335, 135, 100, 20, hWnd, (HMENU)RADIO_A_RECVONLY, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("None"),     WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE | BS_AUTORADIOBUTTON, 445, 135, 100, 20, hWnd, (HMENU)RADIO_A_NONE, GetModuleHandle(NULL), NULL);

	checkbox_audio_mute_ = 
	CreateWindow(TEXT("BUTTON"), TEXT("Audio Mute"), WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE | BS_AUTOCHECKBOX, 555, 135, 100, 20, hWnd, (HMENU)BUTTON_A_MUTE, GetModuleHandle(NULL), NULL);

	//
	// DTMF
	//
	CreateWindow(TEXT("STATIC"), TEXT("DTMF:"),	WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE, 10, 160, 100, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);

	CreateWindow(TEXT("BUTTON"), TEXT("0"),	WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 115, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_0, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("1"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 145, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_1, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("2"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 175, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_2, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("3"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 205, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_3, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("4"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 235, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_4, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("5"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 265, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_5, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("6"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 295, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_6, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("7"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 325, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_7, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("8"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 355, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_8, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("9"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 385, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_9, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("*"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 415, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_AST, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("#"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 445, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_PND, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("A"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 475, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_A, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("B"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 505, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_B, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("C"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 535, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_C, GetModuleHandle(NULL), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("D"), WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 565, 160, 28, 22, hWnd, (HMENU)BUTTON_DTMF_D, GetModuleHandle(NULL), NULL);

	checkbox_auto_answer_ =
		CreateWindow(TEXT("BUTTON"), TEXT("Auto Answer"), WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE | BS_AUTOCHECKBOX, 600, 161, 120, 20, hWnd, (HMENU)BUTTON_AUTO_ANS, GetModuleHandle(NULL), NULL);

	//
	// Initial Settings
	//
	mVideoDirection = acb::MediaDirection::SEND_AND_RECEIVE;
	mAudioDirection = acb::MediaDirection::SEND_AND_RECEIVE;
	::SendMessage(radio_v_sendrecv_, BM_SETCHECK, BST_CHECKED, 0);
	::SendMessage(radio_a_sendrecv_, BM_SETCHECK, BST_CHECKED, 0);

	{
		//FONT SIZE
		HFONT hFont = CreateFont(12, 0, 0, 0, FW_DONTCARE, FALSE, TRUE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("arial"));
		::SendMessage(static_session_, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
	}

	//
	// VIDEO WINDOWS
	//
	static_remoteview_ =
		CreateWindow(TEXT("STATIC"), TEXT("Remote View"),
			WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE,
			10, 190, 450, 350, hWnd, (HMENU)STATIC_REMOTE, GetModuleHandle(NULL), NULL);

	static_localview_ =
		CreateWindow(TEXT("STATIC"), TEXT("Local View"),
			WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE,
			470, 190, LOCAL_VIEW_WIDTH, LOCAL_VIEW_HEIGHT, hWnd, (HMENU)STATIC_LOCAL, GetModuleHandle(NULL), NULL);

	//call
	edit_callnumber_ =
		CreateWindow(TEXT("EDIT"), wss_dialnum.c_str(),
			WS_CHILD | ES_LEFT | WS_BORDER | ES_NUMBER | WS_VISIBLE,
			10, 50, 80, 28, hWnd, (HMENU)EDIT_CALLNUMBER, GetModuleHandle(NULL), NULL);

	button_call_ = 
	CreateWindow(TEXT("BUTTON"), TEXT("CALL"),
		WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE,
		100, 50, 95, 28, hWnd, (HMENU)BUTTON_CALL, GetModuleHandle(NULL), NULL);

	CreateWindow(TEXT("BUTTON"), TEXT("HANGUP"),
		WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE,
		200, 50, 95, 28, hWnd, (HMENU)BUTTON_HANGUP, GetModuleHandle(NULL), NULL);

	CreateWindow(TEXT("BUTTON"), TEXT("HOLD"),
		WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE,
		300, 50, 95, 28, hWnd, (HMENU)BUTTON_HOLD, GetModuleHandle(NULL), NULL);

	CreateWindow(TEXT("BUTTON"), TEXT("UNHOLD"),
		WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE,
		400, 50, 95, 28, hWnd, (HMENU)BUTTON_UNHOLD, GetModuleHandle(NULL), NULL);

	//status
	static_status_ =
		CreateWindow(TEXT("STATIC"), TEXT("STATUS"),
			WS_CHILD | ES_LEFT | WS_BORDER | WS_VISIBLE,
			530, 52, 197, 25, hWnd, (HMENU)STATIC_STATUS, GetModuleHandle(NULL), NULL);

	//combos
	combo_resolution_ =
		CreateWindow(TEXT("COMBOBOX"), TEXT(""),
			CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE,
			470, 410, LOCAL_VIEW_WIDTH, 200, hWnd, (HMENU)COMBO_RESOLUTION, GetModuleHandle(NULL), NULL);

	combo_fps_ =
		CreateWindow(TEXT("COMBOBOX"), TEXT(""),
			//CBS_DROPDOWNLIST | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
			CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE,
			470, 440, LOCAL_VIEW_WIDTH, 200, hWnd, (HMENU)COMBO_FPS, GetModuleHandle(NULL), NULL);

#ifndef VS2013
	combo_videotype_ =
		CreateWindow(TEXT("COMBOBOX"), TEXT(""),
			CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE,
			470, 470, LOCAL_VIEW_WIDTH, 200, hWnd, (HMENU)COMBO_VIDEOTYPE, GetModuleHandle(NULL), NULL);

	combo_videodevice_ =
		CreateWindow(TEXT("COMBOBOX"), TEXT(""),
			CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE,
			470, 500, LOCAL_VIEW_WIDTH, 200, hWnd, (HMENU)COMBO_DEVICE, GetModuleHandle(NULL), NULL);
#endif

	//resolution combobox
	int num = 0;
	while (resolutions[num]) {
		SendMessage(combo_resolution_, CB_INSERTSTRING, num, (LPARAM)resolutions[num]);
		++num;
	}
	SendMessage(combo_resolution_, CB_SETCURSEL, 0, (LPARAM)0);
	app_camera_resolution_idx = 0;

	//fps combobox
	num = 0;
	while (fps_values[num]) {
		SendMessage(combo_fps_, CB_INSERTSTRING, num, (LPARAM)fps_values[num]);
		++num;
	}

	SendMessage(combo_fps_, CB_SETCURSEL, 5, (LPARAM)0);
	app_camera_fps_idx = 5;

#ifndef VS2013
	//videotype combobox
	num = 0;
	while (videotypes[num]) {
		SendMessage(combo_videotype_, CB_INSERTSTRING, num, (LPARAM)videotypes[num]);
		++num;
	}
	SendMessage(combo_videotype_, CB_SETCURSEL, 0, (LPARAM)0);
	app_camera_videotype_idx = 0;
#endif
}

void FcsdkTest::ResizeUI(HWND hWnd, UINT width, UINT height)
{
	MoveWindow(static_localview_,  width - LOCAL_VIEW_WIDTH - 90, 190, LOCAL_VIEW_WIDTH, LOCAL_VIEW_HEIGHT, TRUE);
	MoveWindow(static_remoteview_, 10, 190, width - LOCAL_VIEW_WIDTH - 130, height - 220, TRUE);

	MoveWindow(combo_resolution_,  width - LOCAL_VIEW_WIDTH - 90, 410, LOCAL_VIEW_WIDTH, 200, TRUE);
	MoveWindow(combo_fps_,         width - LOCAL_VIEW_WIDTH - 90, 440, LOCAL_VIEW_WIDTH, 200, TRUE);
	MoveWindow(combo_videotype_,   width - LOCAL_VIEW_WIDTH - 90, 470, LOCAL_VIEW_WIDTH, 200, TRUE);
	MoveWindow(combo_videodevice_, width - LOCAL_VIEW_WIDTH - 90, 500, LOCAL_VIEW_WIDTH, 200, TRUE);
}

void FcsdkTest::ReleaseVideo()
{
	// Release video windows

	//Erase last pictures
	acb::VideoViewImageData dummy = {};
	mLocal->RefreshImage(dummy);
	mRemote->RefreshImage(dummy);
	RedrawLocal();
	RedrawRemote();

	if ((mLocal != nullptr) && mLocal->WindowHandle() != nullptr) {
		mLocal->CleanUpVideoView();
	}
	if ((mRemote != nullptr) && mRemote->WindowHandle() != nullptr) {
		mRemote->CleanUpVideoView();
	}

	if (mLocal)
		mLocal.reset();
	if (mRemote)
		mRemote.reset();

	mLocal = nullptr;
	mRemote = nullptr;

	// Release pipes
	if (mRemotePipeServer)
	{
		mRemotePipeServer->Stop();
		mRemotePipeServer.reset();
	}
	if (mLocalPipeServer)
	{
		mLocalPipeServer->Stop();
		mLocalPipeServer.reset();
	}

	mRemotePipeServer = nullptr;
	mLocalPipeServer = nullptr;
}

//
// Login process with HTTPS
//
#ifdef VS2013
void FcsdkTest::Login_with_https()
{
	std::string host = GetWindowStringText(edit_host_);
	std::string port = GetWindowStringText(edit_port_);
	std::string username = GetWindowStringText(edit_username_);
	std::string password = GetWindowStringText(edit_password_);

	// Determine URL of csdk-sample login service
	std::string url = "https://" + host + ":" + port + "/csdk-sample/SDK/login";

	// Post the REST request to the server
	web::http::client::http_client_config config;
	config.set_validate_certificates(false);
	std::wstring wUrl = s2ws(url);
	web::uri webUri = web::uri::encode_uri(wUrl);
	web::http::client::http_client client(webUri, config);

	// Credentials are posted in JSON format
	web::json::value postValue;
	postValue[L"username"] = web::json::value::string(s2ws(username));
	postValue[L"password"] = web::json::value::string(s2ws(password));
	// Log the JSON request

	client.request(web::http::methods::POST, L"", postValue)
		.then([this, host](web::http::http_response response)   // server and this are needed by 2nd lambda expression, so must be specified for 1st, as well
	{
		// lambda expression which receives the response
		std::cout << "LOGIN RESPONSE: " << response.status_code() << std::endl;
		if (response.status_code() == web::http::status_codes::OK)
		{
			// Return a task containing the JSON body
			return response.extract_json();
		}
		// Return a task containing a null value if response is not 200
		return pplx::task_from_result(web::json::value());
	})
		.then([this, host](pplx::task<web::json::value> previousTask)
	{
		try
		{
			// lambda expression which acts on the return value of the previous lambda
			web::json::value responseValue = previousTask.get();
			// Extract sessionid from JSON response
			web::json::value sessionStringValue = responseValue[L"sessionid"];
			if (!sessionStringValue.is_null())
			{
				// Convert session string to multibyte
				std::wstring wSessionString = sessionStringValue.as_string();
				std::string sessionString = ws2s(wSessionString);
				// Save current server and session string
				sessionStr = sessionString;
				SetWindowStringText(static_session_, sessionStr);
				SetWindowStringText(static_status_, "Login OK.");

				//
				//LOGIN SUCCESS! Let's CreateUC
				//
				StartUC();

			}
			else
			{
				// Inform failure 
			}
		}
		catch (std::exception e)
		{
			std::cout << "LOGIN FAILED: " << e.what() << std::endl;
		}
	});
}
#else
void FcsdkTest::Login_with_https()
{
	std::string host = GetWindowStringText(edit_host_);
	std::string port = GetWindowStringText(edit_port_);
	std::string username = GetWindowStringText(edit_username_);
	std::string password = GetWindowStringText(edit_password_);

	// Determine URL of csdk-sample login service
	std::string url = "https://" + host + ":" + port + "/csdk-sample/SDK/login";

	httplib::SSLClient cli(host, 443);
	//client.set_connection_timeout(0, 300000); //300 milliseconds
	cli.enable_server_certificate_verification(false);

	std::string post_data = "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";

	if (auto res = cli.Post("/csdk-sample/SDK/login", post_data, "application/json")) {
		std::cout << res->status << std::endl;
		std::cout << res->get_header_value("Content-Type") << std::endl;
		std::cout << res->body << std::endl;

		std::string responseBody = res->body;
		// Extract sessionid from JSON response
		Json::CharReaderBuilder builder{};
		auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
		Json::Value root{};
		JSONCPP_STRING errors;
		const auto is_parsed = reader->parse(responseBody.c_str(), responseBody.c_str() + responseBody.length(), &root, &errors);
		std::string sessionString = "";
		if (is_parsed)
		{
			Json::Value tempStr = root;
			sessionString = (tempStr["sessionid"] == NULL) ? "" : tempStr["sessionid"].asString();
		}

		// Save current server and session string
		sessionStr = sessionString;
		SetWindowStringText(static_session_, sessionStr);
		SetWindowStringText(static_status_, "Login OK.");

		//
		//LOGIN SUCCESS! Let's CreateUC
		//
		StartUC();
	}
	else {
		auto err = res.error();
		std::cout << "LOGIN FAILED: " << err << std::endl;
		MessageBox(windowHandle, L"LOGIN FAILED: ", TEXT("Information"), 0);
	}
}
#endif

//
// Create the UC Object
//
void FcsdkTest::StartUC()
{
	// Use a standard STUN server URL in the form of a JSON string
	std::string stunServers("[{\"url\":\"stun:stun.l.google.com:19302\"}]");
	// Create the UC with our session string and STUN servers
	mUC = std::make_unique<acb::UC>(sessionStr, stunServers, this);
	// Start session - notification of success or failure is received asynchronously
	mUC->StartSession();

	std::string sdkVer = "SDK " + mUC->GetSDKVersion();

#ifndef VS2013
	std::string userAgent = mUC->GetUserAgent();
	std::string newTitle = "FCSDK Test [" + sdkVer + " | " + userAgent + "]";
#else
	std::string newTitle = "FCSDK Test (VS2013)  [" + sdkVer + "]";
#endif
	SetWindowStringText(windowHandle, newTitle);

	//Initial Settings
	mUC->GetPhone()->SetPreferredCaptureFrameRate(30);
	mUC->GetPhone()->SetPreferredCaptureResolution(acb::VideoCaptureResolution::RESOLUTION_AUTO);
#ifndef VS2013
	mUC->GetPhone()->SetPreferredVideoType(acb::VideoType::AUTO);
	mUC->GetPhone()->SetAudioEchoCanceller(true);
	mUC->GetPhone()->SetAudioNoiseSuppression(true);
	mUC->GetPhone()->SetAudioNoiseSuppressionLevel(1);
#endif

#ifndef VS2013
	//video device combobox

	//reset
	SendMessage(combo_videodevice_, CB_RESETCONTENT, 0, 0);

	int num_of_devices = mUC->GetPhone()->GetNumberOfVideoDevices();
	for (int i = 0; i < num_of_devices; i++) {
		std::string tmp = mUC->GetPhone()->GetVideoDeviceName(i);
		std::wstring ws;
		ws.assign(tmp.begin(), tmp.end());
		SendMessage(combo_videodevice_, CB_INSERTSTRING, i, (LPARAM)ws.c_str());
	}
	SendMessage(combo_videodevice_, CB_INSERTSTRING, 0, (LPARAM)L"AUTO");
	SendMessage(combo_videodevice_, CB_SETCURSEL, 0, (LPARAM)0);
	app_camera_videodevice_idx = 0;
#endif

	//alternate setting if the first try failed
#ifndef VS2013
	//mUC->GetPhone()->SetResolutionAlternateProperty(640, 480, 12, acb::VideoType::AUTO);
#else
	//mUC->GetPhone()->SetResolutionAlternateProperty(640, 480, 12);
#endif
}

//
// Start calling
//
void FcsdkTest::Call_Start()
{
	// Cannot have more than one call in progress in the sample
	if (mCurrentCallControllerActive)
	{
		acb::Logger::LogError("Dropping attempt to dial because there's already a call in progress");
		MessageBox(windowHandle, L"Dropping attempt to dial  because there's already a call in progress", TEXT("Information"), 0);
		return;
	}

	// FCSDK UC object is created on log in. Cannot make call unless logged in and UC object exists.
	if (mUC == nullptr)
	{
		MessageBox(windowHandle, L"Please log in first", L"Call Failed", MB_OK);
		return;
	}

	ReleaseVideo();

	SetResolution(app_camera_resolution_idx);
	SetFps(app_camera_fps_idx);
	SetVideotype(app_camera_videotype_idx);

	if (isRinging){
		//
		// IncomingCall
		//

		//For Inbound calls, we need to SetPreviewViewName("Local Video") BEFORE mCall->Answer(). (VS2013)
		mCall->SetPreviewViewName("Local Video");
		mCall->SetVideoViewName("Remote Video");

		std::string number = GetWindowStringText(edit_callnumber_);
		mCall->SetListener(this); //Important! to receive Call_END event
		mCall->Answer(mAudioDirection, mVideoDirection);
	}
	else {
		//
		// Outbound Call
		//
		std::string number = GetWindowStringText(edit_callnumber_);
		//passing nullptr as listener here, we'll let the InCallController set itself as the listener
		mCall = mUC->GetPhone()->CreateCall(number, mAudioDirection, mVideoDirection, this);

		//For outbound calls, we should SetPreviewViewName("Local Video") AFTER mCall = mUC->GetPhone()->CreateCall() (VS2013)
		mUC->GetPhone()->SetPreviewViewName("Local Video");
		mCall->SetVideoViewName("Remote Video");
	}

	mLocal = mLocal->CreateVideoView(static_localview_, "Local Video");
	mRemote = mRemote->CreateVideoView(static_remoteview_, "Remote Video");

	mLocal->SetRefreshViewFunction(boost::bind(&FcsdkTest::RedrawLocal, this));
	mRemote->SetRefreshViewFunction(boost::bind(&FcsdkTest::RedrawRemote, this));

	mCurrentCallControllerActive = true;

	// Create pipe servers for video windows
	mRemotePipeServer = std::make_unique<acb::VideoViewPipeServer>("Remote Video", mRemote.get());
	mRemotePipeServer->Start();
	mLocalPipeServer = std::make_unique<acb::VideoViewPipeServer>("Local Video", mLocal.get());
	mLocalPipeServer->Start();
}


//
// End process of calls
//
void FcsdkTest::Call_End()
{
	//MessageBox(windowHandle, L"Call_End", L"Call End", MB_OK);
	acb::Logger::LogDebug("FcsdkTest::Call_End() started");
	if (mCall && mCurrentCallControllerActive){
		mCall->End();
		acb::Logger::LogDebug("FcsdkTest::Call_End() mCall->End(); finished šššš");
	}
	else {
		//MessageBox(windowHandle, L"mCall->End() skipped", L"Call End", MB_OK);
		SetWindowStringText(static_status_, "");
	}

	ResetCallStatus();
	ReleaseVideo();

	acb::Logger::LogDebug("FcsdkTest::Call_End() finished");
}

void FcsdkTest::Call_Hold()
{
	if (mCall && mCurrentCallControllerActive){
		mCall->Hold();
		//Erase last pictures
		acb::VideoViewImageData dummy = {};
		mRemote->RefreshImage(dummy);
		RedrawRemote();
	}
}

void FcsdkTest::Call_Unhold()
{
	if (mCall && mCurrentCallControllerActive){
		mRemotePipeServer = std::make_unique<acb::VideoViewPipeServer>("Remote Video", mRemote.get());
		mRemotePipeServer->Start();
		mCall->Resume();
	}
}

void FcsdkTest::Call_Video_Enable(bool isOn)
{
	if (mCall){
		mCall->EnableLocalVideo(isOn);
	}
}

void FcsdkTest::Call_Audio_Enable(bool isOn)
{
	if (mCall){
		mCall->EnableLocalAudio(isOn);
	}
}


void FcsdkTest::ResetCallStatus()
{
	mCurrentCallControllerActive = false;
	isRinging = false;
	mCall = nullptr;
	//SetWindowStringText(static_status_, "");
}

//
// Movie Display Processing
//
void FcsdkTest::RedrawLocal()
{
	if (mLocal)
	{
		// Invalidate whole client area of the child (the VideoWindow)
		InvalidateRect(mLocal->WindowHandle(), NULL, FALSE);
	}
}

void FcsdkTest::RedrawRemote()
{
	if (mRemote)
	{
		// Invalidate whole client area of the child (the VideoWindow)
		InvalidateRect(mRemote->WindowHandle(), NULL, FALSE);
	}
	else {
		acb::Logger::LogError("FcsdkTest::RedrawRemote mRemote is null, so canceled. ***");
		std::cout << "FcsdkTest::RedrawRemote() mRemote is null, so canceled.";
	}
}

void FcsdkTest::SendDTMF(std::string dtmfString)
{
	if (mCall != nullptr) {
		acb::Logger::LogDebug("SendDtmf:" + dtmfString);
		bool localCallBack = true;
		mCall->SendDTMF(dtmfString, localCallBack);
	}
	else {
		acb::Logger::LogDebug("SendDtmf:" + dtmfString + "GetPhone() is nullptr! ***********");
	}
}

//
// UCListner. We will catch the events here.
//
void FcsdkTest::OnSessionStarted()
{
	std::cout << "UC session started" << std::endl;
	// Set this as a listener for incoming calls
	mUC->GetPhone()->SetListener((acb::PhoneListener*) this);
}

void FcsdkTest::OnSessionNotStarted()
{
	MessageBox(windowHandle, L"Login was successful, but session was not established. Log out and log back in again.",
		L"Session Failed", MB_OK);
}

void FcsdkTest::OnConnectionLost()
{
	ResetCallStatus();

	std::cout << "UC connection lost" << std::endl;
	mUC.reset(nullptr);
	MessageBox(windowHandle, L"Connection was lost. Please login again.", TEXT("Information"), 0);
}

//
// PhoneListener override
//

//
// Handle notification of an incoming call.
//
void FcsdkTest::OnIncomingCall(acb::CallPtr call)
{
	acb::Logger::LogDebug("Incoming call");
	
	// Cannot have more than one call in progress in the sample
	if (mCurrentCallControllerActive)
	{
		acb::Logger::LogError("Dropping attempt to receive call because there's already a call in progress");
		return;
	}

	isRinging = true;
	//erase call number field. (othewise, the user might call out unexpectedly.)
	SetWindowStringText(edit_callnumber_, "");

	mCall = call;
	mUC->GetPhone()->SetPreviewViewName("Local Video");
	mCall->SetPreviewViewName("Local Video");
	mCall->SetVideoViewName("Remote Video");

	std::string from = call->GetRemoteAddress();
	std::string message = "Incoming Call from " + from;
	SetWindowStringText(static_status_, message);

	if (isAutoAnswer == true) {
		Sleep(1000);
		SendMessage(button_call_, BM_CLICK, 0, 0);
	}
	// ***** IMPORTANT YOU CAN NOT DO THIS. IT WILL MAKE APP CRASH *****
	//	mCall->Answer(mAudioDirection, mVideoDirection);
	// You can "mCall->Answer()" at receive button handler.
}

void FcsdkTest::CallStatusBusy()
{
	SetWindowStringText(static_status_, "Busy");
	ResetCallStatus();
}

void FcsdkTest::CallStatusEnded()
{
	acb::Logger::LogDebug("FcsdkTest::OnCallStatusEnded() started");
	SetWindowStringText(static_status_, "Call Ended");
	Call_End();
	acb::Logger::LogDebug("FcsdkTest::OnCallStatusEnded() finished");
}

//
// CallListner
//
void FcsdkTest::OnStatusChanged(acb::CallStatus newStatus){
	acb::Logger::LogDebug("FcsdkTest::OnStatusChanged() started");
	switch (newStatus)
	{
	case acb::CallStatus::UNINITIALIZED:
		acb::Logger::LogDebug("status change UNINITIALIZED ");
		SetWindowStringText(static_status_, "UNINITIALIZED");
		break;
	case acb::CallStatus::SETUP:
		acb::Logger::LogDebug("status change SETUP ");
		SetWindowStringText(static_status_, "SETUP");
		break;
	case acb::CallStatus::ALERTING:
		acb::Logger::LogDebug("status change ALERTING ");
		SetWindowStringText(static_status_, "ALERTING");
		break;
	case acb::CallStatus::RINGING:
		acb::Logger::LogDebug("status change RINGING ");
		SetWindowStringText(static_status_, "RINGING");
		break;
	case acb::CallStatus::MEDIA_PENDING:
		acb::Logger::LogDebug("status change MEDIA_PENDING ");
		SetWindowStringText(static_status_, "MEDIA_PENDING");
		break;
	case acb::CallStatus::IN_CALL:
		acb::Logger::LogDebug("status change IN_CALL ");
		SetWindowStringText(static_status_, "IN_CALL");
		break;
	case acb::CallStatus::TIMED_OUT:
		acb::Logger::LogDebug("status change TIMED_OUT ");
		SetWindowStringText(static_status_, "TIMED_OUT");
		break;
	case acb::CallStatus::BUSY:
		acb::Logger::LogDebug("status change BUSY ");
		SetWindowStringText(static_status_, "BUSY");
		CallStatusBusy();
		break;
	case acb::CallStatus::NOT_FOUND:
		acb::Logger::LogDebug("status change NOT_FOUND ");
		SetWindowStringText(static_status_, "NOT_FOUND");
		break;
	case acb::CallStatus::CALL_ERROR:
		acb::Logger::LogDebug("status change CALL_ERROR ");
		SetWindowStringText(static_status_, "CALL_ERROR");
		break;
	case acb::CallStatus::ENDED:
		acb::Logger::LogDebug("status change ENDED ");
		SetWindowStringText(static_status_, "ENDED");
		CallStatusEnded();
		break;
	default:
		acb::Logger::LogDebug("status change **UNRECOGNISED** ");
		SetWindowStringText(static_status_, "**UNRECOGNISED**");
		break;
	}
	acb::Logger::LogDebug("FcsdkTest::OnStatusChanged() finished");
};
void FcsdkTest::OnDialFailed(){
	acb::Logger::LogDebug("FcsdkTest::OnDialFailed()");
	ResetCallStatus();
};
void FcsdkTest::OnCallFailed(){
	acb::Logger::LogDebug("FcsdkTest::OnCallFailed()");
	ResetCallStatus();
};
void FcsdkTest::OnRemoteMediaStream(){
	acb::Logger::LogDebug("FcsdkTest::OnRemoteMediaStream()");
};
void FcsdkTest::OnLocalMediaStream(){
	acb::Logger::LogDebug("FcsdkTest::OnLocalMediaStream()");
};
void FcsdkTest::OnInboundQualityChange(int inboundQuality){
	acb::Logger::LogDebug("FcsdkTest::OnInboundQualityChange()");
};
void FcsdkTest::OnOutboundAudioFailure(){
	acb::Logger::LogDebug("FcsdkTest::OnOutboundAudioFailure()");
};
void FcsdkTest::OnOutboundVideoFailure(){
	acb::Logger::LogDebug("FcsdkTest::OnOutboundVideoFailure()");
};
void FcsdkTest::OnRemoteDisplayNameChanged(std::string displayname){
	acb::Logger::LogDebug("FcsdkTest::OnRemoteDisplayNameChanged()");
};
void FcsdkTest::OnRemoteHeld(){
	acb::Logger::LogDebug("FcsdkTest::OnRemoteHeld()");
};
void FcsdkTest::OnRemoteUnheld(){
	acb::Logger::LogDebug("FcsdkTest::OnRemoteUnheld()");
};


