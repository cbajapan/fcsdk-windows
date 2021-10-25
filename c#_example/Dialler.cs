using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

using FCSDKCLR;

namespace WebRTCClientWinManaged
{
    /// <summary>
    /// The interface that represents the available interactions from the
    /// dialler controller.
    /// </summary>
    public interface IDiallerView
    {
        void AddLogMessage(string logMessage);
        void SetController(DiallerController dc);
        void SetLoggedIn(bool loggedIn);
        void OnRemoteHangupCall();
        /* A call has just become active */
        void SetCallActive();
        /* A call has just become inactive */
        void SetCallInactive();
        /* The phone has just started ringing */
        void SetPhoneRinging();
        /* Allow the hangup function to be activated */
        void EnableHangupCapability();
        /* Set the version information so that it can be displayed. */
        void SetSDKVersion(String sdkVersion);
    }

    public partial class Dialler : Form, IDiallerView
    {
        private IDiallerWindowListener mController;

        public Dialler()
        {
            InitializeComponent();
            // The code below is to make sure items word wrap if they are too big to
            // fit into the list box window.
            // Ref: http://stackoverflow.com/questions/17613613/are-there-anyway-to-make-listbox-items-to-word-wrap-if-string-width-is-higher-th
            listBoxActivity.DrawMode = System.Windows.Forms.DrawMode.OwnerDrawVariable;
            listBoxActivity.MeasureItem += ListBoxMeasureItem;
            listBoxActivity.DrawItem += ListBoxDrawItem;
        }

        // The code below is to make sure items word wrap if they are too big to
        // fit into the list box window.
        private void ListBoxMeasureItem(object sender, MeasureItemEventArgs e)
        {
            e.ItemHeight = (int)e.Graphics.MeasureString(
                listBoxActivity.Items[e.Index].ToString(), 
                listBoxActivity.Font, listBoxActivity.Width).Height;
        }

        // The code below is to make sure items word wrap if they are too big to
        // fit into the list box window.
        private void ListBoxDrawItem(object sender, DrawItemEventArgs e)
        {
            e.DrawBackground();
            e.DrawFocusRectangle();
            if (e.Index >= 0 && e.Index < listBoxActivity.Items.Count)
            {
                e.Graphics.DrawString(listBoxActivity.Items[e.Index].ToString(),
                    e.Font, new SolidBrush(e.ForeColor), e.Bounds);
            }
        }
        
        private void buttonLogin_Click(object sender, EventArgs e)
        {
            mController.OnMenuEventLogin();
        }

        private void buttonLogout_Click(object sender, EventArgs e)
        {
            mController.OnMenuEventLogout();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "1";
        }

        private void button2_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "2";
        }

        private void button3_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "3";
        }

        private void button4_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "4";
        }

        private void button5_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "5";
        }

        private void button6_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "6";
        }

        private void button7_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "7";
        }

        private void button8_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "8";
        }

        private void button9_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "9";
        }

        private void buttonStar_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "*";
        }

        private void button0_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "0";
        }

        private void buttonHash_Click(object sender, EventArgs e)
        {
            textBoxPhoneNum.Text += "#";
        }

        private void buttonBackspace_Click(object sender, EventArgs e)
        {
            if (textBoxPhoneNum.Text.Length > 0)
            {
                string num = textBoxPhoneNum.Text;
                num = num.Substring(0, num.Length - 1);
                textBoxPhoneNum.Text = num;
            }
        }

        private void buttonAnswer_Click(object sender, EventArgs e)
        {
            mController.OnAnswerCall(this.Bounds, audioDirection(), videoDirection());
        }

        private void buttonReject_Click(object sender, EventArgs e)
        {
            mController.OnRejectCall();
        }

        static readonly Dictionary<Boolean, Dictionary<Boolean, CLI_MediaDirection>> mMediaDirection;

        static Dialler()
        {
            Dictionary<Boolean, CLI_MediaDirection> sendTrue = new Dictionary<Boolean, CLI_MediaDirection>();
            sendTrue[true] = CLI_MediaDirection.SEND_AND_RECEIVE;
            sendTrue[false] = CLI_MediaDirection.SEND_ONLY;
            Dictionary<Boolean, CLI_MediaDirection> sendFalse = new Dictionary<Boolean, CLI_MediaDirection>();
            sendFalse[true] = CLI_MediaDirection.RECEIVE_ONLY;
            sendFalse[false] = CLI_MediaDirection.NONE;
            mMediaDirection = new Dictionary<Boolean, Dictionary<Boolean, CLI_MediaDirection>>();
            mMediaDirection[true] = sendTrue;
            mMediaDirection[false] = sendFalse;
        }

        private CLI_MediaDirection audioDirection()
        {
            return mMediaDirection[checkBoxSendAudio.Checked][checkBoxReceiveAudio.Checked];
        }

        private CLI_MediaDirection videoDirection()
        {
            return mMediaDirection[checkBoxSendVideo.Checked][checkBoxReceiveVideo.Checked];
        }
        
        private void buttonHangUp_Click(object sender, EventArgs e)
        {
            mController.OnHangupCall();
            buttonDial.Enabled = true;
            buttonHangUp.Enabled = false;
            // These below are needed for the case where somebody dials
            // this client then hangs up before we can answer or reject
            // the call.
            buttonAnswer.Enabled = false;
            buttonReject.Enabled = false;
            checkBoxReceiveAudio.Enabled = true;
            checkBoxReceiveVideo.Enabled = true;
            checkBoxSendAudio.Enabled = true;
            checkBoxSendVideo.Enabled = true;
        }

        private void buttonDial_Click(object sender, EventArgs e)
        {
            mController.OnDialNumber(textBoxPhoneNum.Text, this.Bounds, audioDirection(), videoDirection());
            buttonDial.Enabled = false;
            checkBoxReceiveAudio.Enabled = false;
            checkBoxReceiveVideo.Enabled = false;
            checkBoxSendAudio.Enabled = false;
            checkBoxSendVideo.Enabled = false;
        }

        public void SetController(DiallerController dc)
        {
            mController = dc;
        }

        public void SetLoggedIn(bool loggedIn)
        {
            // Ensure update occurs on the UI thread!
            if (InvokeRequired)
            {
                this.Invoke(new Action<bool>(SetLoggedIn), loggedIn);
            }
            else
            {
                buttonLogout.Enabled = loggedIn;
                buttonLogin.Enabled = !loggedIn;
                buttonDial.Enabled = loggedIn;
                buttonHangUp.Enabled = false;
                buttonAnswer.Enabled = false;
                buttonReject.Enabled = false;
                buttonAedDemo.Enabled = loggedIn;
            }
        }

        public void SetCallActive()
        {
            // Ensure update occurs on the UI thread!
            if (InvokeRequired)
            {
                this.Invoke(new Action(SetCallActive));
            }
            else
            {
                buttonDial.Enabled = false;
                buttonHangUp.Enabled = true;
                buttonAnswer.Enabled = false;
                buttonReject.Enabled = false;
                checkBoxReceiveAudio.Enabled = false;
                checkBoxReceiveVideo.Enabled = false;
                checkBoxSendAudio.Enabled = false;
                checkBoxSendVideo.Enabled = false;
            }
        }

        public void SetCallInactive()
        {
            // Ensure update occurs on the UI thread!
            if (InvokeRequired)
            {
                this.Invoke(new Action(SetCallInactive));
            }
            else
            {
                buttonDial.Enabled = true;
                buttonHangUp.Enabled = false;
                buttonAnswer.Enabled = false;
                buttonReject.Enabled = false;
                checkBoxReceiveAudio.Enabled = true;
                checkBoxReceiveVideo.Enabled = true;
                checkBoxSendAudio.Enabled = true;
                checkBoxSendVideo.Enabled = true;
                // Make sure the call window is also closed.
                mController.OnHangupCall();
            }
        }

        public void SetPhoneRinging()
        {
            // Ensure update occurs on the UI thread!
            if (InvokeRequired)
            {
                this.Invoke(new Action(SetPhoneRinging));
            }
            else
            {
                buttonDial.Enabled = false;
                buttonHangUp.Enabled = false;
                buttonAnswer.Enabled = true;
                buttonReject.Enabled = true;
            }
        }

        public void AddLogMessage(string logMessage)
        {
            // Ensure update occurs on the UI thread!
            if (listBoxActivity.InvokeRequired)
            {
                this.Invoke(new Action<string>(AddLogMessage), logMessage);
            }
            else
            {
                listBoxActivity.Items.Add(logMessage);
            }
        }

        public void OnRemoteHangupCall()
        {
            // This signal has probably come from a non-UI thread. We need to make sure
            // the hang-up request happens on the UI thread.
            if (InvokeRequired)
            {
                this.Invoke(new Action<object, EventArgs>(buttonHangUp_Click), this, null);
            }
            else
            {
                buttonHangUp_Click(this, null);
            }
        }

        private void buttonAedDemo_Click(object sender, EventArgs e)
        {
            mController.StartAEDDemo();
        }

        public void EnableHangupCapability()
        {
            // This signal has probably come from a non-UI thread. We need to make sure
            // the hang-up request happens on the UI thread.
            if (InvokeRequired)
            {
                this.Invoke(new Action(EnableHangupCapability));
            }
            else
            {
                buttonHangUp.Enabled = true;
            }
        }

        public void SetSDKVersion(string sdkVersion)
        {
            labelSDKVersion.Text = "SDK Version " + sdkVersion;
        }

    }
}
