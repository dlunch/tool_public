using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Ui
{
    public static class Native
    {
        public delegate void OnMessage([MarshalAs(UnmanagedType.LPWStr)] String message);
        public delegate void OnCommand(IntPtr handle, [MarshalAs(UnmanagedType.LPWStr)] String command, [MarshalAs(UnmanagedType.LPWStr)] String value);

        [DllImport("Native.dll", EntryPoint = "native_initialize")]
        public static extern IntPtr Initialize(OnMessage onMessage, OnCommand onCommand);

        [DllImport("Native.dll", EntryPoint = "native_evaluate")]
        public static extern void Evaluate(IntPtr handle, [MarshalAs(UnmanagedType.LPWStr)] String command, IntPtr module);

        [DllImport("Native.dll", EntryPoint = "native_getAvailableModules")]
        private static extern int GetAvailableModules_(IntPtr handle, [MarshalAs(UnmanagedType.LPWStr)] StringBuilder result);

        [DllImport("Native.dll", EntryPoint = "native_startModule")]
        public static extern IntPtr StartModule(IntPtr handle, [MarshalAs(UnmanagedType.LPWStr)] String module);

        [DllImport("Native.dll", EntryPoint = "native_reloadModule")]
        public static extern IntPtr ReloadModule(IntPtr handle, [MarshalAs(UnmanagedType.LPWStr)] String module, IntPtr moduleHandle);

        /// <summary>
        /// helper for GetAvailableModules
        /// </summary>
        /// <param name="handle"></param>
        /// <returns></returns>
        public static IEnumerable<String> GetAvailableModules(IntPtr handle)
        {
            int count = GetAvailableModules_(handle, null);

            StringBuilder result = new StringBuilder(256);
            GetAvailableModules_(handle, result);

            return result.ToString().Split('\u0001');
        }
    }
}
