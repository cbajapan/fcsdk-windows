//
//   FCSDK Win32 WINDOWS MINIMAL SAMPLE
//   (C)2021 Communication Business Avenue, Inc. / SD
//
#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include <string>
#include <iostream>
#include <memory>

//for fcsdk
#include "UC.h"
#include "Phone.h"
#include "Call.h"
#include "MediaDirection.h"
#include "VideoView.h"
#include "VideoWindow.h"
#include "ImageDataPipe.h"

#define BUTTON_1         9001
#define BUTTON_2         9002
#define BUTTON_LOGIN     9003
#define BUTTON_CALL      9004
#define BUTTON_HANGUP    9005
#define EDIT_HOST        9006
#define EDIT_PORT        9007
#define EDIT_CALLNUMBER  9008
#define EDIT_USERNAME    9009
#define EDIT_PASSWORD    9010
#define STATIC_SESSION   9021
#define STATIC_LOCAL     9022
#define STATIC_REMOTE    9023
#define STATIC_STATUS    9024
#define RADIO_V_SENDRECV 9025
#define RADIO_V_SENDONLY 9026
#define RADIO_V_RECVONLY 9027
#define RADIO_V_NONE     9028
#define RADIO_A_SENDRECV 9029
#define RADIO_A_SENDONLY 9030
#define RADIO_A_RECVONLY 9031
#define RADIO_A_NONE     9032
#define BUTTON_DTMF_0    9033
#define BUTTON_DTMF_1    9034
#define BUTTON_DTMF_2    9035
#define BUTTON_DTMF_3    9036
#define BUTTON_DTMF_4    9037
#define BUTTON_DTMF_5    9038
#define BUTTON_DTMF_6    9039
#define BUTTON_DTMF_7    9040
#define BUTTON_DTMF_8    9041
#define BUTTON_DTMF_9    9042
#define BUTTON_DTMF_A    9043
#define BUTTON_DTMF_B    9044
#define BUTTON_DTMF_C    9045
#define BUTTON_DTMF_D    9046
#define BUTTON_DTMF_AST  9047
#define BUTTON_DTMF_PND  9048
#define BUTTON_HOLD      9049
#define BUTTON_UNHOLD    9050
#define BUTTON_V_MUTE    9051
#define BUTTON_A_MUTE    9052
#define COMBO_RESOLUTION 9053
#define COMBO_FPS        9054
#define COMBO_VIDEOTYPE  9055
#define COMBO_DEVICE     9056
#define BUTTON_AUTO_ANS  9057

#define LOCAL_VIEW_WIDTH  200
#define LOCAL_VIEW_HEIGHT 200


//
// VideoViewPipeServer(Helper) Class
//

namespace acb
{
	/*
	Class derived from SDK ImageDataPipe class. The ImageDataPipe manages
	the data passing through the pipe, and calls the SendImageToView method.
	This class overrides SendImageToView to send the image data to a
	VideoWindow; subclasses in another application might send it to an ActiveX
	control or some other class which can get an HDC for a VideoView to
	render it on, but in a windows application, we send it to a window.

	The VideoViewPipeServer is associated with a single VideoWindow, which
	in turn owns a single VideoView. The VideoViewPipeServer is associated
	with an FCSDK ImageDataPipeClient by its name (provided in its constructor)
	so that there is a single channel for data from the ImageDataPipeClient in
	the FCSDK, through the VideoViewPipeServer, through the VideoWindow, and to
	the VideoView in the FCSDK which does the actual rendering of the image.

	How to use:
	1) create Local and Remote views with the VideoWindow class.
	2) connect the Views and WebRTC with the PipeServers below.
	3) SDK will handle the rest of the part.
	mRemotePipeServer = std::make_unique<acb::VideoViewPipeServer>("Remote Video", mRemote.get());
	mRemotePipeServer->Start();
	mLocalPipeServer = std::make_unique<acb::VideoViewPipeServer>("Local Video", mLocal.get());
	mLocalPipeServer->Start();
	*/

	class VideoViewPipeServer : public ImageDataPipe
	{
	private:
		VideoWindow* view;
	protected:
		/*
		Overridden to send the new image data to the window. The current
		image data has to be retrieved from the superclass.
		*/
		void VideoViewPipeServer::SendImageToView()
		{
			if (view)
			{
				// Send the new image on to the associated VideoWindow
				view->RefreshImage(GetImageData());
			}
		}

	public:
		VideoViewPipeServer::VideoViewPipeServer(std::string name, VideoWindow* view) : ImageDataPipe(name)
		{
			// Save VideoWindow
			this->view = view;
		}

		virtual ~VideoViewPipeServer() {};

	};
} // namespace acb

//
// Application main
//

class FcsdkTest : public acb::UCListener, public acb::PhoneListener, public acb::CallListener
{
private:
	//A static router that will receive the Win32 message loop. This will call WndProc().
	static LRESULT CALLBACK MessageRouter(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	//Write your message logics below.
	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	UINT form_width;
	UINT form_height;

	//This app's window handle
	HWND windowHandle;

	// Single instance of the Fusion Client SDK used for making and managing calls
	// FCSDK Parent Class
	std::unique_ptr<acb::UC> mUC;

	acb::CallPtr mCall;
	std::string sessionStr;

	//Login account
	std::string HOST_URL;
	std::string USERNAME;
	std::string PASSWORD;
	std::string DIALNUM;

	//camera setting
	int app_camera_resolution_idx;
	int app_camera_fps_idx;
	int app_camera_videotype_idx;
	int app_camera_videodevice_idx;

	HWND edit_host_;
	HWND edit_port_;
	HWND edit_username_;
	HWND edit_password_;
	HWND edit_callnumber_;
	HWND static_session_;
	HWND static_localview_;
	HWND static_remoteview_;
	HWND static_status_;
	HWND radio_v_sendrecv_;
	HWND radio_a_sendrecv_;
	HWND checkbox_video_mute_;
	HWND checkbox_audio_mute_;
	HWND checkbox_auto_answer_;
	HWND button_call_;

	HWND combo_resolution_;
	HWND combo_fps_;
	HWND combo_videotype_;
	HWND combo_videodevice_;

	bool isRinging;
	bool isAutoAnswer;

	acb::MediaDirection mAudioDirection;
	acb::MediaDirection mVideoDirection;

	// Flag to indicate whether the controller for the current call is active
	bool mCurrentCallControllerActive = false;

	// UI element for video from the local camera (what will be sent to the recipient)
	std::unique_ptr<acb::VideoWindow> mLocal;
	// UI element for video sent by the recipient of the call
	std::unique_ptr<acb::VideoWindow> mRemote;

	// ImageDataPipe receiving video from the remote end
	std::unique_ptr<acb::VideoViewPipeServer> mRemotePipeServer;
	// ImageDataPipe receiving video from the local camera
	std::unique_ptr<acb::VideoViewPipeServer> mLocalPipeServer;

	//functions
	void CreateUI(HWND hWnd);
	void ResizeUI(HWND hWnd, UINT width, UINT height);
	void StartUC();
	void Login_with_https();
	//std::unique_ptr<acb::VideoWindow> CreateVideoView(HWND parent, std::wstring name);
	void Call_Start();
	void Call_End();
	void Call_Hold();
	void Call_Unhold();
	void Call_Video_Enable(bool isOn);
	void Call_Audio_Enable(bool isOn);
	void RedrawLocal();
	void RedrawRemote();
	void ReleaseVideo();
	void ResetCallStatus();
	void CallStatusBusy();
	void CallStatusEnded();
	void SendDTMF(std::string dtmf);
	void SetResolution(int num);
	void SetFps(int num);
	void SetVideotype(int num);
	void SetVideoDevice(int num);

	//PhoneListener
	void OnIncomingCall(acb::CallPtr call) override;

	//UCListner
	void OnSessionStarted() override;
	void OnSessionNotStarted() override;
	void OnConnectionLost() override;

	//CallListner
	void OnDialFailed() override;
	void OnCallFailed() override;
	void OnStatusChanged(acb::CallStatus newStatus) override;
	void OnRemoteMediaStream() override;
	void OnLocalMediaStream() override;
	void OnInboundQualityChange(int inboundQuality) override;
	void OnOutboundAudioFailure() override;
	void OnOutboundVideoFailure() override;
	void OnRemoteDisplayNameChanged(std::string displayname) override;
	void OnRemoteHeld() override;
	void OnRemoteUnheld() override;

public:
	FcsdkTest(int nCmdLn);
	int Run();
};