using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Drawing;

using FCSDKCLR;

namespace WebRTCClientWinManaged
{
    public interface IInCallWindowListener
    {
        void OnInCallWindowDestroyed();
    }

    public class InCallController : IDisposable, ICLI_CallListener, IInCallWindowListener
    {
        private CLI_Call mCall;
        private DiallerController mDiallerController;
        private bool mDisposed;

        public InCallController(CLI_Call call, DiallerController diallerController)
        {
            mCall = call;
            mCall.SetListener(this);
            mDiallerController = diallerController;
        }

        // Public implementation of Dispose pattern callable by consumers. 
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (mDisposed) return;

            if (disposing)
            {
                // Release pipes
                if (mRemotePipeServer != null)
                {
                    mRemotePipeServer.Stop();
                    mRemotePipeServer.Dispose();
                    mRemotePipeServer = null;
                }

                if (mLocalPipeServer != null)
                {
                    mLocalPipeServer.Stop();
                    mLocalPipeServer.Dispose();
                    mLocalPipeServer = null;
                }

                // Dispose of the video window.
                if (mCallWnd != null)
                {
                    mCallWnd.Dispose();
                    mCallWnd = null;
                }
            }
            // Free unmanaged objects here
            if (mCall != null)
            {
                mCall.Dispose();
                mCall = null;
            }

            mDisposed = true;
        }

        /// <summary>
        /// Calculate the desired position and size of the call window.
        /// </summary>
        /// <param name="diallerRect">Position and size of the dialler window. (Currently ignored.)</param>
        /// <returns>A Rectangle object containing the desired coordinates of the call window.</returns>
        public Rectangle CalculateCallWindowRect(Rectangle diallerRect)
        {
            if (mDisposed) throw new ObjectDisposedException(GetType().FullName);
            // Find the resolution of the primary screen.
            Rectangle screenBounds = System.Windows.Forms.Screen.PrimaryScreen.Bounds;
            // We now need to calculate a base size for the window we will display.
            // We want the area to be approx 1/4 of the screen size.
            // The aspect ratio needs to be 4:3.
            // The area of the screen is of course given by width * height.
            // To work out the width and height of the window:
            // width * height = area
            // width = 4 * height / 3
            // therefore (4 * height / 3) * height = area
            // height * height = 3 * area / 4
            // height = sqrt(3*area/4)
            // and the width is determined from the height.
            // We want to play with round numbers and also we want the video preview
            // to have integer width and height (which currently has a quarter of the
            // width and height of the remote video) so round the height to the nearest
            // multiple of 12 and then calculate the width.
            double desiredArea = screenBounds.Width * screenBounds.Height / 4.0;
            int height = (int)(Math.Round(Math.Sqrt(3.0 * desiredArea / 4.0) / 12.0) * 12.0);
            int width = height / 3 * 4;
            Rectangle callWinRect = new Rectangle(
                screenBounds.Left + (screenBounds.Width - width) / 2,
                screenBounds.Top + (screenBounds.Height - height) / 2,
                width,
                height);

            return callWinRect;
        }

        private InCallWindow mCallWnd;
        private VideoViewPipeServer mRemotePipeServer;
        private VideoViewPipeServer mLocalPipeServer;
        /*
        Starts the UI and connects up the named pipes with the similarly named ones for video
        rendering.
        Note that the local video does not currently work as no video is being delivered on that
        pipe, though the pipe server and video window classes are retained.
        */
        public void StartUINextToDiallerRect(Rectangle bounds)
        {
            if (mDisposed) throw new ObjectDisposedException(GetType().FullName);
            // Set the names of the remote video pipe in the call in FCSDK           
            mCall.SetVideoViewName("Remote Video");
            // Make an InCallWindow for this call, with this as the listener to receive call events
            mCallWnd = new InCallWindow(this);
            // Create the actual window as a child of the dialler, but to its right
            Rectangle callWndRect = CalculateCallWindowRect(bounds);
            mCallWnd.Show();
            mCallWnd.DesktopLocation = new Point(callWndRect.Left, callWndRect.Top);
            mCallWnd.ClientSize = new Size(callWndRect.Width, callWndRect.Height);

            // Set the refresh view function in the VideoWindow (which will set it in its VideoView)
            mCallWnd.GetLocalVideoWindow().SetRefreshViewFunction(
                new RedrawWindow(mCallWnd.GetLocalVideoWindow()));
            mCallWnd.GetRemoteVideoWindow().SetRefreshViewFunction(
                new RedrawWindow(mCallWnd.GetRemoteVideoWindow()));
            // Create pipe servers for video windows
            mCallWnd.GetRemoteVideoWindow().WindowName = "Remote Video";
            mRemotePipeServer = new VideoViewPipeServer("Remote Video", mCallWnd.GetRemoteVideoWindow());
            mRemotePipeServer.Start();
            mCallWnd.GetLocalVideoWindow().WindowName = "Local Video";
            mLocalPipeServer = new VideoViewPipeServer("Local Video", mCallWnd.GetLocalVideoWindow());
            mLocalPipeServer.Start();
        }

        class RedrawWindow : ICLI_ViewRefresher
        {
            private VideoWindow mCallWnd;

            public RedrawWindow(VideoWindow callWnd)
            {
                mCallWnd = callWnd;
            }

            public void RefreshViewFunc()
            {
                mCallWnd.Invalidate();
            }
        }

        public void OnCallFailed()
        {
            // Just report it.
            mDiallerController.LogEvent("InCallController.OnCallFailed called");
        }

        public void OnDialFailed()
        {
            // Just report it.
            mDiallerController.LogEvent("InCallController.OnDialFailed called");
        }

        public void OnInboundQualityChange(int inboundQuality)
        {
            // Just report it.
            mDiallerController.LogEvent("Inbound call quality changed to " + inboundQuality);
        }

        public void OnLocalMediaStream()
        {
            // Just report it.
            mDiallerController.LogEvent("InCallController.OnLocalMediaStream called");
        }

        public void OnRemoteMediaStream()
        {
            // Just report it.
            mDiallerController.LogEvent("InCallController.OnRemoteMediaStream called");
        }

        public void OnRemoteDisplayNameChanged(String newName)
        {
            // Just report it.
            mDiallerController.LogEvent("OnRemoteDisplayNameChanged called");
            mDiallerController.LogEvent("New name: " + newName);
        }

        public void OnRemoteHeld()
        {
            // Just report it.
            mDiallerController.LogEvent("OnRemoteHeld called");
        }

        public void OnRemoteUnheld()
        {
            // Just report it.
            mDiallerController.LogEvent("OnRemoteUnheld called");
        }

        public void OnOutboundAudioFailure()
        {
            // Just report it.
            mDiallerController.LogEvent("InCallController.OnOutboundAudioFailure called");
        }

        public void OnOutboundVideoFailure()
        {
            // Just report it.
            mDiallerController.LogEvent("InCallController.OnOutboundVideoFailure called");
        }

        public void OnStatusChanged(CLI_CallStatus newStatus)
        {
            if (mDisposed)
            {
                // Just report it.
                mDiallerController.LogEvent("InCallController.OnStatusChange called after object disposed");
                mDiallerController.LogEvent("Status was " + newStatus);
                return;
            }
            switch (newStatus)
            {
            case CLI_CallStatus.UNINITIALIZED:
                mDiallerController.LogEvent("status change UNINITIALIZED");
                break;
            case CLI_CallStatus.SETUP:
                mDiallerController.LogEvent("status change SETUP");
                break;
            case CLI_CallStatus.ALERTING:
                mDiallerController.LogEvent("status change ALERTING");
                break;
            case CLI_CallStatus.RINGING:
                mDiallerController.LogEvent("status change RINGING");
                // We report the event to the dialler controller so that hang-up function can be enabled.
                mDiallerController.OnEntryToRingingState();
                break;
            case CLI_CallStatus.MEDIA_PENDING:
                mDiallerController.LogEvent("status change MEDIA_PENDING");
                break;
            case CLI_CallStatus.IN_CALL:
                mDiallerController.LogEvent("status change IN_CALL");
                // We report the event to the dialler controller so that hang-up function can be enabled.
                mDiallerController.OnEntryToInCallState();
                break;
            case CLI_CallStatus.TIMED_OUT:
                mDiallerController.LogEvent("status change TIMED_OUT");
                break;
            case CLI_CallStatus.BUSY:
                mDiallerController.LogEvent("status change BUSY");
                mDiallerController.OnCallStatusBusy();
                break;
            case CLI_CallStatus.NOT_FOUND:
                mDiallerController.LogEvent("status change NOT_FOUND");
                break;
            case CLI_CallStatus.CALL_ERROR:
                mDiallerController.LogEvent("status change CALL_ERROR");
                break;
            case CLI_CallStatus.ENDED:
                mDiallerController.LogEvent("status change ENDED");
                mDiallerController.OnRemoteHangupCall();
                break;
            default:
                mDiallerController.LogEvent("status change **UNRECOGNISED**");
                break;
            }
        }

        public void OnInCallWindowDestroyed()
        {
            // Report it.
            mDiallerController.LogEvent("InCallController.OnInCallWindowDestroyed called");
            if (mDisposed)
            {
                mDiallerController.LogEvent("InCallController has been disposed");
                return;
            }
            // Hangup the call.
            mCall.End();
            // Notify DiallerController that call controller is ending.
            mDiallerController.OnCallControllerEnded();
        }

        public void OnHangupCall()
        {
            // Report it.
            mDiallerController.LogEvent("InCallController.OnHangupCall called");
            if (mDisposed)
            {
                mDiallerController.LogEvent("InCallController has been disposed");
                return;
            }
            // Hangup the call.
            mCall.End();
            // Notify DiallerController that call controller is ending.
            mDiallerController.OnCallControllerEnded();
        }

        public void OnRejectCallPressed()
        {
            // Report it.
            mDiallerController.LogEvent("InCallController.OnRejectCallPressed called");
            if (mDisposed)
            {
                mDiallerController.LogEvent("InCallController has been disposed");
                return;
            }
            // Hangup the call.
            mCall.End();
            // Notify DiallerController that call controller is ending.
            mDiallerController.OnCallControllerEnded();
        }


        internal void OnAnswerCallPressed(CLI_MediaDirection audioDirection, CLI_MediaDirection videoDirection)
        {
            // Report it.
            mDiallerController.LogEvent("InCallController.OnAnswerCallPressed called");
            if (mDisposed)
            {
                mDiallerController.LogEvent("InCallController has been disposed");
                return;
            }
            // Answer the call.
            mCall.Answer(audioDirection, videoDirection);
        }
    }
}
