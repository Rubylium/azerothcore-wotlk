using System;
using System.Runtime.InteropServices;

namespace Evolutions
{
    // The Explorer folder picker (IFileDialog with FOS_PICKFOLDERS): WPF has none, and the old WinForms tree
    // dialog does not fit a modern window
    public static class FolderPicker
    {
        const uint FOS_PICKFOLDERS = 0x20;
        const uint FOS_FORCEFILESYSTEM = 0x40;
        const uint FOS_PATHMUSTEXIST = 0x800;
        const uint SIGDN_FILESYSPATH = 0x80058000;
        const int ERROR_CANCELLED = unchecked((int)0x800704C7);

        public static string Pick(IntPtr owner, string title, string initialFolder)
        {
            var dialog = (IFileDialog)new FileOpenDialog();
            try
            {
                dialog.GetOptions(out uint options);
                dialog.SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
                dialog.SetTitle(title);
                if (!string.IsNullOrEmpty(initialFolder) &&
                    SHCreateItemFromParsingName(initialFolder, IntPtr.Zero, typeof(IShellItem).GUID,
                        out IShellItem folder) == 0)
                    dialog.SetFolder(folder);

                int result = dialog.Show(owner);
                if (result == ERROR_CANCELLED)
                    return null;
                Marshal.ThrowExceptionForHR(result);

                dialog.GetResult(out IShellItem item);
                item.GetDisplayName(SIGDN_FILESYSPATH, out IntPtr path);
                try
                {
                    return Marshal.PtrToStringUni(path);
                }
                finally
                {
                    Marshal.FreeCoTaskMem(path);
                }
            }
            finally
            {
                Marshal.ReleaseComObject(dialog);
            }
        }

        [DllImport("shell32.dll", CharSet = CharSet.Unicode, PreserveSig = true)]
        static extern int SHCreateItemFromParsingName(string path, IntPtr bindContext,
            [MarshalAs(UnmanagedType.LPStruct)] Guid riid, out IShellItem item);

        [ComImport, Guid("DC1C5A9C-E88A-4dde-A5A1-60F82A20AEF7")]
        class FileOpenDialog
        {
        }

        [ComImport, Guid("42f85136-db7e-439c-85f1-e4075d135fc8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        interface IFileDialog
        {
            [PreserveSig] int Show(IntPtr parent);
            void SetFileTypes(uint count, IntPtr filterSpec);
            void SetFileTypeIndex(uint index);
            void GetFileTypeIndex(out uint index);
            void Advise(IntPtr events, out uint cookie);
            void Unadvise(uint cookie);
            void SetOptions(uint options);
            void GetOptions(out uint options);
            void SetDefaultFolder(IShellItem item);
            void SetFolder(IShellItem item);
            void GetFolder(out IShellItem item);
            void GetCurrentSelection(out IShellItem item);
            void SetFileName([MarshalAs(UnmanagedType.LPWStr)] string name);
            void GetFileName([MarshalAs(UnmanagedType.LPWStr)] out string name);
            void SetTitle([MarshalAs(UnmanagedType.LPWStr)] string title);
            void SetOkButtonLabel([MarshalAs(UnmanagedType.LPWStr)] string text);
            void SetFileNameLabel([MarshalAs(UnmanagedType.LPWStr)] string label);
            void GetResult(out IShellItem item);
        }

        [ComImport, Guid("43826D1E-E718-42EE-BC55-A1E261C37BFE"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        interface IShellItem
        {
            void BindToHandler(IntPtr bindContext, [MarshalAs(UnmanagedType.LPStruct)] Guid handler,
                [MarshalAs(UnmanagedType.LPStruct)] Guid riid, out IntPtr result);
            void GetParent(out IShellItem parent);
            void GetDisplayName(uint type, out IntPtr name);
            void GetAttributes(uint mask, out uint attributes);
            void Compare(IShellItem other, uint hint, out int order);
        }
    }
}
