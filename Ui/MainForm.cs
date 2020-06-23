using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Ui
{
    public partial class MainForm : Form
    {
        private IntPtr _nativeHandle;
        private IntPtr _module;
        private String _moduleName;
        private Native.OnMessage _onMessage; //keep reference to delegate
        private Native.OnCommand _onCommand;

        public MainForm()
        {
            InitializeComponent();
        }

        private void MainForm_Load(object sender, EventArgs e)
        {
#if !DEBUG
            Directory.SetCurrentDirectory(Path.GetDirectoryName(Application.ExecutablePath));
#endif
            _onMessage = OnMessage;
            _onCommand = OnCommand;
            _nativeHandle = Native.Initialize(_onMessage, _onCommand);

            var modules = Native.GetAvailableModules(_nativeHandle);
            _moduleName = modules.FirstOrDefault();
            if (_moduleName != "")
            {
                _module = Native.StartModule(_nativeHandle, _moduleName);
                if (_module == (IntPtr) 0)
                    MessageBox.Show("어태치 실패. 관리자로 켜보셈.");
            }
            else
                MessageBox.Show("게임이 안켜져있는듯.");
        }

        private void PrintAndExecute(String command)
        {
            OnMessage(">>> " + command + "\n");
            Native.Evaluate(_nativeHandle, command, _module);
        }

        private void OnMessage(String message)
        {
            Invoke((MethodInvoker) delegate
            {
                MessageOutput.AppendText(message);
                MessageOutput.SelectionStart = MessageOutput.Text.Length;
                MessageOutput.ScrollToCaret();
            });
        }

        private void OnCommand(IntPtr handle, String command, String value)
        {
            if (command == "add_button")
            {
                Invoke((MethodInvoker) delegate
                {
                    Button button = new Button();
                    button.Text = value;
                    button.Click += (sender, args) => Native.Evaluate(_nativeHandle, $"module.execute_button('{value}')", handle);
                    flowLayoutPanel1.Controls.Add(button);
                });
            }
        }

        private void CommandInput_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
            {
                PrintAndExecute(CommandInput.Text);
                CommandInput.Text = "";
            }
        }

        private void reloadButton_Click(object sender, EventArgs e)
        {
            flowLayoutPanel1.Controls.Clear();
            _module = Native.ReloadModule(_nativeHandle, _moduleName, _module);
        }

        private void MainForm_FormClosed(object sender, FormClosedEventArgs e)
        {
            // TODO suicide gracefully.
            Process.GetCurrentProcess().Kill();
        }
    }
}
