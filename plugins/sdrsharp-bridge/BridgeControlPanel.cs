using System;
using System.Drawing;
using System.Windows.Forms;
using SDRSharp.Common;

namespace SDRSharp.AstroSeriesBridge
{
    internal sealed class BridgeControlPanel : UserControl
    {
        private readonly IqBridgeProcessor _processor;
        private readonly ISharpControl _control;
        private readonly DopplerControlReader _doppler;
        private readonly CheckBox _enabled;
        private readonly CheckBox _autoDoppler;
        private readonly Label _status;
        private readonly Timer _timer;

        public BridgeControlPanel(ISharpControl control, IqBridgeProcessor processor)
        {
            _control = control;
            _processor = processor;
            _doppler = new DopplerControlReader();
            AutoSize = true;
            Padding = new Padding(8);
            TableLayoutPanel layout = new TableLayoutPanel
            {
                Dock = DockStyle.Top,
                AutoSize = true,
                ColumnCount = 1,
                RowCount = 5
            };
            Controls.Add(layout);
            layout.Controls.Add(new Label
            {
                Text = "阿斯图系列本地 I/Q 与多普勒",
                AutoSize = true,
                Font = new Font(Font, FontStyle.Bold)
            });
            _enabled = new CheckBox
            {
                Text = "Enable local complex I/Q bridge",
                Checked = processor.Enabled,
                AutoSize = true
            };
            _enabled.CheckedChanged += delegate { processor.Enabled = _enabled.Checked; };
            layout.Controls.Add(_enabled);
            _autoDoppler = new CheckBox
            {
                Text = "启用自动多普勒调谐",
                Checked = true,
                AutoSize = true
            };
            layout.Controls.Add(_autoDoppler);
            layout.Controls.Add(new Label
            {
                Text = "Shared memory: " + IqBridgeProcessor.MappingName,
                AutoSize = true
            });
            _status = new Label { AutoSize = true };
            layout.Controls.Add(_status);
            _timer = new Timer { Interval = 500 };
            _timer.Tick += delegate { UpdateStatus(); };
            _timer.Start();
            UpdateStatus();
        }

        private void UpdateStatus()
        {
            string bridge = _processor.Enabled
                ? string.Format("ACTIVE  {0:0} -> {1:0} sample/s  {2:N0} samples",
                    _processor.CurrentSampleRate, _processor.BridgeSampleRate,
                    _processor.SamplesWritten)
                : "DISABLED";
            long targetHz;
            long correctionHz;
            if (!_autoDoppler.Checked)
            {
                _status.Text = bridge + "\r\nDOPPLER OFF";
            }
            else if (_doppler.TryRead(out targetHz, out correctionHz))
            {
                try
                {
                    if (Math.Abs(_control.Frequency - targetHz) >= 1)
                        _control.Frequency = targetHz;
                    _status.Text = string.Format(
                        "{0}\r\nDOPPLER {1:+0;-0;0} Hz  ->  {2:0.000000} MHz",
                        bridge, correctionHz, targetHz / 1000000.0);
                }
                catch
                {
                    _status.Text = bridge + "\r\nDOPPLER tuning unavailable";
                }
            }
            else
            {
                _status.Text = bridge + "\r\nDOPPLER waiting for tracker";
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _timer.Dispose();
                _doppler.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
