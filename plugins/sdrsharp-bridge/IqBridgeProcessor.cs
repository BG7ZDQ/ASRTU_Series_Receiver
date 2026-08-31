using System;
using System.Diagnostics;
using System.IO.MemoryMappedFiles;
using System.Threading;
using SDRSharp.Radio;

namespace SDRSharp.AstroSeriesBridge
{
    internal unsafe sealed class IqBridgeProcessor : IIQProcessor, IStreamProcessor,
        IBaseProcessor, IDisposable
    {
        internal const string MappingName = @"Local\ASRTU_IQ_BRIDGE_V1";
        private const uint Magic = 0x42514941;
        private const uint Version = 1;
        private const int HeaderBytes = 64;
        private const int CapacitySamples = 262144;
        private const int ComplexBytes = 8;
        private const double OutputSampleRate = 48000.0;
        private const long MappingBytes = HeaderBytes + (long)CapacitySamples * ComplexBytes;

        private readonly MemoryMappedFile _mapping;
        private readonly MemoryMappedViewAccessor _view;
        private readonly object _stateLock = new object();
        private byte* _base;
        private bool _disposed;
        private bool _enabled = true;
        private double _inputSampleRate = OutputSampleRate;
        private long _inputCount;
        private double _nextOutputPosition;
        private Complex _previousSample;
        private bool _havePreviousSample;

        public IqBridgeProcessor()
        {
            _mapping = MemoryMappedFile.CreateOrOpen(
                MappingName, MappingBytes, MemoryMappedFileAccess.ReadWrite);
            _view = _mapping.CreateViewAccessor(0, MappingBytes,
                                                MemoryMappedFileAccess.ReadWrite);
            byte* pointer = null;
            _view.SafeMemoryMappedViewHandle.AcquirePointer(ref pointer);
            _base = pointer + _view.PointerOffset;
            InitializeHeader();
        }

        public bool Enabled
        {
            get { return Volatile.Read(ref _enabled); }
            set
            {
                lock (_stateLock)
                {
                    Volatile.Write(ref _enabled, value);
                    if (_base != null)
                        Volatile.Write(ref *(int*)(_base + 40), value ? 1 : 0);
                }
            }
        }

        public double SampleRate
        {
            set
            {
                if (double.IsNaN(value) || double.IsInfinity(value) ||
                    value < 8000.0 || value > 20000000.0)
                    return;
                lock (_stateLock)
                {
                    _inputSampleRate = value;
                    _inputCount = 0;
                    _nextOutputPosition = 0;
                    _havePreviousSample = false;
                    if (_base != null)
                        *(double*)(_base + 32) = value;
                }
            }
        }

        public double CurrentSampleRate
        {
            get { lock (_stateLock) { return _inputSampleRate; } }
        }
        public double BridgeSampleRate { get { return OutputSampleRate; } }
        public long SamplesWritten
        {
            get
            {
                lock (_stateLock)
                {
                    return _base == null ? 0 :
                        Volatile.Read(ref *(long*)(_base + 24));
                }
            }
        }

        public void Process(Complex* buffer, int length)
        {
            if (!Volatile.Read(ref _enabled) || Volatile.Read(ref _disposed) ||
                buffer == null || length <= 0)
                return;

            lock (_stateLock)
            {
                if (Volatile.Read(ref _disposed) || _base == null)
                    return;
                ulong writeIndex = (ulong)Volatile.Read(ref *(long*)(_base + 24));
                byte* data = _base + HeaderBytes;
                if (!_havePreviousSample)
                {
                    _previousSample = buffer[0];
                    _havePreviousSample = true;
                    _nextOutputPosition = 0;
                }

                long blockStart = _inputCount;
                long blockEnd = blockStart + length - 1;
                double step = _inputSampleRate / OutputSampleRate;
                while (_nextOutputPosition <= blockEnd)
                {
                    long lowerGlobal = (long)Math.Floor(_nextOutputPosition);
                    float fraction = (float)(_nextOutputPosition - lowerGlobal);
                    Complex lower;
                    Complex upper;
                    if (lowerGlobal < blockStart)
                    {
                        lower = _previousSample;
                        upper = buffer[0];
                    }
                    else
                    {
                        int lowerOffset = (int)(lowerGlobal - blockStart);
                        int upperOffset = Math.Min(lowerOffset + 1, length - 1);
                        lower = buffer[lowerOffset];
                        upper = buffer[upperOffset];
                    }

                    int ringOffset = (int)(writeIndex % CapacitySamples);
                    float* destination = (float*)(data + (long)ringOffset * ComplexBytes);
                    destination[0] = lower.Real + (upper.Real - lower.Real) * fraction;
                    destination[1] = lower.Imag + (upper.Imag - lower.Imag) * fraction;
                    writeIndex++;
                    _nextOutputPosition += step;
                }

                _previousSample = buffer[length - 1];
                _inputCount += length;
                Thread.MemoryBarrier();
                Volatile.Write(ref *(long*)(_base + 24), (long)writeIndex);
                Volatile.Write(ref *(long*)(_base + 48), Stopwatch.GetTimestamp());
            }
        }

        private void InitializeHeader()
        {
            for (int i = 0; i < HeaderBytes; ++i)
                _base[i] = 0;
            *(uint*)(_base + 0) = Magic;
            *(uint*)(_base + 4) = Version;
            *(double*)(_base + 8) = OutputSampleRate;
            *(uint*)(_base + 16) = CapacitySamples;
            *(uint*)(_base + 20) = ComplexBytes;
            *(double*)(_base + 32) = _inputSampleRate;
            *(int*)(_base + 40) = _enabled ? 1 : 0;
            *(long*)(_base + 48) = Stopwatch.GetTimestamp();
            *(int*)(_base + 56) = System.Diagnostics.Process.GetCurrentProcess().Id;
            Thread.MemoryBarrier();
        }

        public void Dispose()
        {
            lock (_stateLock)
            {
                if (Volatile.Read(ref _disposed))
                    return;
                Volatile.Write(ref _disposed, true);
                if (_base != null)
                {
                    Volatile.Write(ref *(int*)(_base + 40), 0);
                    _view.SafeMemoryMappedViewHandle.ReleasePointer();
                    _base = null;
                }
            }
            _view.Dispose();
            _mapping.Dispose();
        }
    }
}
