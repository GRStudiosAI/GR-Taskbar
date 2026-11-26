# Original release verification tooling: GR Studios AI LLC, 2026.
if (-not ('GRBuildVerification' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
public static class GRBuildVerification {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr LoadLibraryExW(string name, IntPtr file, uint flags);
    [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern IntPtr FindResourceW(IntPtr module, IntPtr name, IntPtr type);
    [DllImport("kernel32.dll")] static extern uint SizeofResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")] static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")] static extern IntPtr LockResource(IntPtr resource);
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    struct TrustFile { public uint size; [MarshalAs(UnmanagedType.LPWStr)] public string path; public IntPtr file, subject; }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    struct TrustData {
        public uint size; public IntPtr policy, sip; public uint ui, revocation, choice;
        public IntPtr file; public uint stateAction; public IntPtr state;
        [MarshalAs(UnmanagedType.LPWStr)] public string url;
        public uint flags, context; public IntPtr settings;
    }
    [DllImport("wintrust.dll", ExactSpelling=true)] static extern int WinVerifyTrust(IntPtr window, ref Guid action, ref TrustData data);
    public static uint TrustStatus(string path) {
        TrustFile file = new TrustFile { size=(uint)Marshal.SizeOf(typeof(TrustFile)), path=path };
        IntPtr memory = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(TrustFile)));
        try {
            Marshal.StructureToPtr(file, memory, false);
            TrustData data = new TrustData { size=(uint)Marshal.SizeOf(typeof(TrustData)), ui=2, choice=1, file=memory, flags=0x1000 };
            Guid action = new Guid("00AAC56B-CD44-11D0-8CC2-00C04FC295EE");
            return unchecked((uint)WinVerifyTrust(new IntPtr(-1), ref action, ref data));
        } finally { Marshal.DestroyStructure(memory, typeof(TrustFile)); Marshal.FreeHGlobal(memory); }
    }
    static byte[] Resource(IntPtr module, int name, int type) {
        IntPtr resource = FindResourceW(module, new IntPtr(name), new IntPtr(type));
        if (resource == IntPtr.Zero) throw new InvalidDataException("Missing icon resource");
        uint size = SizeofResource(module, resource);
        byte[] bytes = new byte[size];
        Marshal.Copy(LockResource(LoadResource(module, resource)), bytes, 0, bytes.Length);
        return bytes;
    }
    public static bool IconMatches(string executable, string icon) {
        byte[] ico = File.ReadAllBytes(icon);
        IntPtr module = LoadLibraryExW(executable, IntPtr.Zero, 2);
        if (module == IntPtr.Zero) throw new InvalidDataException("Cannot load installer resources");
        try {
            byte[] group = Resource(module, 101, 14);
            int count = BitConverter.ToUInt16(ico, 4);
            if (BitConverter.ToUInt16(group, 4) != count) return false;
            for (int i=0; i<count; i++) {
                int a=6+16*i, b=6+14*i;
                int length=(int)BitConverter.ToUInt32(ico,a+8), offset=(int)BitConverter.ToUInt32(ico,a+12);
                if (ico[a]!=group[b] || ico[a+1]!=group[b+1]) return false;
                byte[] actual = Resource(module, BitConverter.ToUInt16(group,b+12), 3);
                if (actual.Length != length) return false;
                for (int j=0;j<length;j++) if (actual[j]!=ico[offset+j]) return false;
            }
            return true;
        } finally { FreeLibrary(module); }
    }
}
'@
}

function Test-GRSignature {
    param([string]$Path, [string]$Thumbprint, [ValidateSet('SelfSigned','Trusted')][string]$Mode)
    $signature = Get-AuthenticodeSignature -LiteralPath $Path
    if (-not $signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -ne $Thumbprint) {
        throw "Missing or unexpected signing certificate: $Path"
    }
    $trust = [GRBuildVerification]::TrustStatus($Path)
    $untrustedRoot = [Convert]::ToUInt32('800B0109',16)
    if ($trust -ne 0 -and -not ($Mode -eq 'SelfSigned' -and $trust -eq $untrustedRoot)) {
        throw ('Signature verification failed: {0}; Windows status 0x{1:X8}' -f $Path,$trust)
    }
    if (-not $signature.TimeStamperCertificate) { throw "Missing RFC 3161 timestamp: $Path" }
    [pscustomobject]@{
        File = [IO.Path]::GetFileName($Path)
        Signer = $signature.SignerCertificate.Subject
        Thumbprint = $signature.SignerCertificate.Thumbprint
        WindowsTrustStatus = ('0x{0:X8}' -f $trust)
        AutomaticallyTrusted = ($trust -eq 0)
        SignatureDigestChecked = $true
        TimestampAuthority = $signature.TimeStamperCertificate.Subject
    }
}
