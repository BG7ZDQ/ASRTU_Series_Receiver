using System;
using System.Diagnostics;
using System.IO.MemoryMappedFiles;
using System.Threading;
using SDRSharp.AstroSeriesBridge;

internal static class BridgePluginTest
{
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
    }

    public static int Main(string[] args)
    {
        try
        {
            // Avoid disrupting an actual running station or a retained map.
            try
            {
                using (MemoryMappedFile.OpenExisting(IqBridgeProcessor.MappingName))
                {
                    Console.WriteLine("SKIP: live I/Q mapping exists");
                    return 77;
                }
            }
            catch (System.IO.FileNotFoundException) { }
            bool created;
            using (var owner = new EventWaitHandle(false, EventResetMode.ManualReset,
                @"Local\ASRTU_BRIDGE_OWNER_V1", out created))
            {
                if (!created) return 77;
                using (var rejected = new IqBridgeProcessor())
                    Check(!rejected.Enabled && rejected.StartError != null,
                          "managed plugin ignored native ownership token");
            }
            using (var first = new IqBridgeProcessor())
            using (var second = new IqBridgeProcessor())
            {
                Check(first.Enabled && !second.Enabled, "managed contenders both started");
                using (var child = Process.Start(new ProcessStartInfo(args[0], "--probe-default")
                    { UseShellExecute = false, CreateNoWindow = true }))
                {
                    Check(child.WaitForExit(5000), "native probe timed out");
                    Check(child.ExitCode == 0, "native plugin ignored managed ownership token");
                }
                using (var map = MemoryMappedFile.OpenExisting(IqBridgeProcessor.MappingName))
                using (var view = map.CreateViewAccessor())
                {
                    Check(view.ReadInt32(40) == 1, "rejected managed plugin disabled producer");
                    first.Enabled = false;
                    Check(view.ReadInt32(40) == 0 && view.ReadInt32(56) == 0,
                          "managed stop retained ownership");
                    second.Enabled = true;
                    Check(second.Enabled, "managed handover failed");
                    first.Enabled = true;
                    Check(!first.Enabled, "managed restart stole ownership");
                    Check(view.ReadInt32(40) == 1, "failed restart disabled other plugin");
                }
            }
            Console.WriteLine("PASS: SDR# contention, C#/C++ ownership interoperability, stop/retry/handover");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
    }
}
