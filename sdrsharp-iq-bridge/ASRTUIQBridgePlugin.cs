using System.Windows.Forms;
using SDRSharp.Common;
using SDRSharp.Radio;

namespace SDRSharp.AstroSeriesBridge
{
    public sealed class AstroSeriesBridgePlugin : ISharpPlugin
    {
        private ISharpControl _control;
        private IqBridgeProcessor _processor;
        private BridgeControlPanel _panel;

        public string DisplayName { get { return "阿斯图系列本地 I/Q 与多普勒"; } }

        public UserControl Gui
        {
            get
            {
                if (_panel == null)
                    _panel = new BridgeControlPanel(_control, _processor);
                return _panel;
            }
        }

        public void Initialize(ISharpControl control)
        {
            _control = control;
            _processor = new IqBridgeProcessor();
            control.RegisterStreamHook(_processor, ProcessorType.DecimatedAndFilteredIQ);
        }

        public void Close()
        {
            if (_panel != null)
                _panel.Dispose();
            if (_control != null && _processor != null)
                _control.UnregisterStreamHook(_processor);
            if (_processor != null)
                _processor.Dispose();
            _panel = null;
            _processor = null;
            _control = null;
        }
    }
}
