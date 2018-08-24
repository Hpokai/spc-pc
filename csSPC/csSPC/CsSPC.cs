using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.Management;

namespace csSPC
{
    public class CsSPC
    {
        [DllImport(".\\cSPC.dll", EntryPoint = "CreateLibSPC", CallingConvention = CallingConvention.Cdecl)]
        static public extern IntPtr CreateLibSPC();

        [DllImport(".\\cSPC.dll", EntryPoint = "Close", CallingConvention = CallingConvention.Cdecl)]
        static public extern void Close(IntPtr ctor);

        [DllImport(".\\cSPC.dll", EntryPoint = "IsKeyError", CallingConvention = CallingConvention.Cdecl)]
        static public extern bool IsKeyError(IntPtr ctor);

        [DllImport(".\\cSPC.dll", EntryPoint = "test", CallingConvention = CallingConvention.Cdecl)]
        static public extern int test(IntPtr ctor, int a, int b, char[] port_name, bool run, bool key_error);

        public IntPtr lib_spc = IntPtr.Zero;

        public CsSPC()
        {
            //use the functions
            this.lib_spc = CreateLibSPC();
            this.test(1, 2);
        }

        public void Close()
        {
            Console.WriteLine("csSPC: Close");
            Close(this.lib_spc);
        }

        public bool IsKeyError()
        {
            Console.WriteLine("csSPC: IsKeyError");
            return IsKeyError(this.lib_spc);
        }

        public int test(int a, int b)
        {
            //if (this.lib_spc != null)
            //{
            Console.WriteLine("csSPC: test");

            char[] arr = {'\0'};
            string port_name = AutodetectSerialPort("CH340");
            Console.WriteLine("csSPC: port_name-> {0}", port_name);
            bool run = false;
            bool key_error = false;
            if (null != port_name)
            {
                arr = port_name.ToCharArray(0, port_name.Length);
                run = true;
            }
            else
            {
                Console.WriteLine("csSPC: NO Dongle");
                key_error = true;
            }

            //Console.WriteLine("csSPC: as dog---------------------------------------->");
            //foreach (char c in arr)
            //    Console.Write(c);
            //Console.WriteLine("");

            return test(this.lib_spc, a, b, arr, run, key_error);
            //}
        }

        #region Auto Connect To Arduino
        private string AutodetectSerialPort(string port_name)
        {
            ManagementScope connectionScope = new ManagementScope();
            SelectQuery serialQuery = new SelectQuery("SELECT * FROM Win32_PnPEntity");        //  Win32_SerialPort
            ManagementObjectSearcher searcher = new ManagementObjectSearcher(connectionScope, serialQuery);

            try
            {
                foreach (ManagementObject item in searcher.Get())
                {
                    string desc = item["Description"].ToString();
                    string deviceId = item["Name"].ToString();

                    //Console.WriteLine("csSPC: desc @@{0}@@, Name =={1}==", desc, deviceId);
                    //if (desc.Contains("Arduino")) 
                    if (desc.Contains(port_name))
                    {
                        int start = deviceId.IndexOf("(");
                        int end = deviceId.IndexOf(")");
                        string ret = deviceId.Substring(start + 1, (end - start - 1));
                        //Console.WriteLine("csSPC: COM ====== {0}", temp);                      
                        return ret;
                    }
                }
            }
            catch {/* Do Nothing */}

            return null;
        }
        #endregion

    }
}
