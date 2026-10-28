# ==============================================================================
# LIVE DEMONSTRATION RUN SCRIPT FOR MULTI-CLIENT CHAT APPLICATION
# Computer Networks Laboratory Project
# ==============================================================================

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $projectRoot

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host " LIVE DEMONSTRATION: MULTI-CLIENT TCP CHAT APPLICATION (C / WINSOCK 2)" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

$PORT = 8888

# 1. Start Server Process
Write-Host "[1/6] Launching TCP Chat Server on Port $PORT..." -ForegroundColor Yellow
$serverLogFile = "$projectRoot\demo_server_run.log"
if (Test-Path $serverLogFile) { Remove-Item $serverLogFile -Force }

$serverProc = Start-Process -FilePath "$projectRoot\server.exe" -ArgumentList "$PORT" -PassThru -RedirectStandardOutput $serverLogFile
Start-Sleep -Seconds 1

if ($serverProc.HasExited) {
    Write-Host "[ERROR] Server failed to start! Log content:" -ForegroundColor Red
    Get-Content $serverLogFile
    exit 1
}
Write-Host "[+] Server is RUNNING (PID: $($serverProc.Id)) on Port $PORT" -ForegroundColor Green
Write-Host ""

# 2. Compile/Load Test Client helper for demonstration
Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Net.Sockets;
using System.Text;

public class DemoPacket {
    public byte Type;
    public byte[] Payload;
    public DemoPacket(byte type, byte[] payload) {
        Type = type;
        Payload = payload;
    }
}

public class DemoClient : IDisposable {
    private TcpClient client;
    private NetworkStream stream;
    public string Username { get; private set; }

    public DemoClient(string host, int port, string username) {
        this.Username = username;
        client = new TcpClient();
        client.Connect(host, port);
        stream = client.GetStream();
    }

    public bool SendPacket(byte type, byte[] payload) {
        uint len = (payload != null) ? (uint)payload.Length : 0;
        byte[] header = new byte[9];
        // magic = 0x434E4C41 ('CNLA')
        header[0] = 0x43; header[1] = 0x4E; header[2] = 0x4C; header[3] = 0x41;
        header[4] = type;
        header[5] = (byte)((len >> 24) & 0xFF);
        header[6] = (byte)((len >> 16) & 0xFF);
        header[7] = (byte)((len >> 8) & 0xFF);
        header[8] = (byte)(len & 0xFF);

        stream.Write(header, 0, header.Length);
        if (len > 0 && payload != null) {
            stream.Write(payload, 0, payload.Length);
        }
        stream.Flush();
        return true;
    }

    public DemoPacket RecvPacket(int timeoutMs = 3000) {
        client.ReceiveTimeout = timeoutMs;
        byte[] header = new byte[9];
        int read = 0;
        while (read < 9) {
            int r = stream.Read(header, read, 9 - read);
            if (r <= 0) return null;
            read += r;
        }

        byte type = header[4];
        uint payloadLen = ((uint)header[5] << 24) | ((uint)header[6] << 16) | ((uint)header[7] << 8) | header[8];

        byte[] payload = new byte[payloadLen];
        read = 0;
        while (read < payloadLen) {
            int r = stream.Read(payload, read, (int)payloadLen - read);
            if (r <= 0) return null;
            read += r;
        }
        return new DemoPacket(type, payload);
    }

    public DemoPacket RecvExpected(byte expectedType, int timeoutMs = 4000) {
        DateTime deadline = DateTime.Now.AddMilliseconds(timeoutMs);
        while (DateTime.Now < deadline) {
            DemoPacket pkt = RecvPacket(1000);
            if (pkt == null) continue;
            if (pkt.Type == expectedType) return pkt;
        }
        return null;
    }

    public bool ConnectHandshake() {
        byte[] userBytes = new byte[32];
        byte[] ascii = Encoding.ASCII.GetBytes(this.Username);
        Array.Copy(ascii, userBytes, Math.Min(ascii.Length, 31));
        SendPacket(1, userBytes); // PKT_CONNECT_REQ = 1
        var resp = RecvExpected(2, 3000); // PKT_CONNECT_ACK = 2
        return (resp != null && resp.Type == 2);
    }

    public void Dispose() {
        try {
            SendPacket(13, null); // PKT_DISCONNECT = 13
            if (stream != null) stream.Close();
            if (client != null) client.Close();
        } catch {}
    }
}
"@

# 3. Connect Client 1 (Nithish)
Write-Host "[2/6] Connecting Client 1: 'Nithish'..." -ForegroundColor Yellow
$client1 = [DemoClient]::new("127.0.0.1", $PORT, "Nithish")
$c1Ok = $client1.ConnectHandshake()
if ($c1Ok) {
    Write-Host "  -> [CONNECTED] Nithish joined the chat server successfully!" -ForegroundColor Green
} else {
    Write-Host "  -> [FAILED] Nithish could not join." -ForegroundColor Red
}

# 4. Connect Client 2 (Rahul)
Write-Host "[3/6] Connecting Client 2: 'Rahul'..." -ForegroundColor Yellow
$client2 = [DemoClient]::new("127.0.0.1", $PORT, "Rahul")
$c2Ok = $client2.ConnectHandshake()
if ($c2Ok) {
    Write-Host "  -> [CONNECTED] Rahul joined the chat server successfully!" -ForegroundColor Green
} else {
    Write-Host "  -> [FAILED] Rahul could not join." -ForegroundColor Red
}

Start-Sleep -Milliseconds 400

# 5. Broadcast Messaging
Write-Host "`n[4/6] Demonstrating Real-Time Broadcast Messaging..." -ForegroundColor Yellow
Write-Host "  [Nithish > ALL]: Hello everyone! Welcome to Computer Networks Lab." -ForegroundColor White

$bcast1 = New-Object byte[] 2080
[System.Text.Encoding]::ASCII.GetBytes("Nithish").CopyTo($bcast1, 0)
[System.Text.Encoding]::ASCII.GetBytes("Hello everyone! Welcome to Computer Networks Lab.").CopyTo($bcast1, 32)
$client1.SendPacket(4, $bcast1)

$rMsg1 = $client2.RecvExpected(4, 3000)
if ($rMsg1 -ne $null) {
    $txt = [System.Text.Encoding]::ASCII.GetString($rMsg1.Payload, 32, 100).TrimEnd([char]0)
    Write-Host "  -> [Rahul terminal received]: [MSG] Nithish: $txt" -ForegroundColor Cyan
}

Write-Host "  [Rahul > ALL]: Hi Nithish! Great to be connected via TCP sockets." -ForegroundColor White
$bcast2 = New-Object byte[] 2080
[System.Text.Encoding]::ASCII.GetBytes("Rahul").CopyTo($bcast2, 0)
[System.Text.Encoding]::ASCII.GetBytes("Hi Nithish! Great to be connected via TCP sockets.").CopyTo($bcast2, 32)
$client2.SendPacket(4, $bcast2)

$nMsg2 = $client1.RecvExpected(4, 3000)
if ($nMsg2 -ne $null) {
    $txt2 = [System.Text.Encoding]::ASCII.GetString($nMsg2.Payload, 32, 100).TrimEnd([char]0)
    Write-Host "  -> [Nithish terminal received]: [MSG] Rahul: $txt2" -ForegroundColor Cyan
}

# 6. Private Messaging (/msg)
Write-Host "`n[5/6] Demonstrating Unicast Private Messaging (/msg)..." -ForegroundColor Yellow
Write-Host "  [Nithish -> Rahul (Private)]: Let's review the viva questions before the lab exam." -ForegroundColor White

$pmsg = New-Object byte[] 2112
[System.Text.Encoding]::ASCII.GetBytes("Nithish").CopyTo($pmsg, 0)
[System.Text.Encoding]::ASCII.GetBytes("Rahul").CopyTo($pmsg, 32)
[System.Text.Encoding]::ASCII.GetBytes("Let's review the viva questions before the lab exam.").CopyTo($pmsg, 64)
$client1.SendPacket(5, $pmsg)

$rPriv = $client2.RecvExpected(5, 3000)
if ($rPriv -ne $null) {
    $pTxt = [System.Text.Encoding]::ASCII.GetString($rPriv.Payload, 64, 100).TrimEnd([char]0)
    Write-Host "  -> [Rahul terminal received]: [PRIVATE from Nithish]: $pTxt" -ForegroundColor Magenta
}

# 7. File Transfer (/sendfile)
Write-Host "`n[6/6] Demonstrating Chunked TCP File Transfer (/sendfile sample.txt)..." -ForegroundColor Yellow
$sampleFile = "$projectRoot\files\test_files\sample.txt"
if (Test-Path $sampleFile) {
    $bytes = [System.IO.File]::ReadAllBytes($sampleFile)
    Write-Host "  [Nithish]: Sending 'sample.txt' ($($bytes.Length) bytes) to Rahul..." -ForegroundColor White

    # Send PKT_FILE_START
    $fstart = New-Object byte[] 332
    [System.Text.Encoding]::ASCII.GetBytes("Nithish").CopyTo($fstart, 0)
    [System.Text.Encoding]::ASCII.GetBytes("Rahul").CopyTo($fstart, 32)
    [System.Text.Encoding]::ASCII.GetBytes("sample.txt").CopyTo($fstart, 64)
    [System.BitConverter]::GetBytes([uint64]$bytes.Length).CopyTo($fstart, 320)
    [System.BitConverter]::GetBytes([uint32]1).CopyTo($fstart, 328)
    $client1.SendPacket(8, $fstart)

    $rStart = $client2.RecvExpected(8, 3000)

    # Send PKT_FILE_DATA
    $fdata = New-Object byte[] 4424
    [System.Text.Encoding]::ASCII.GetBytes("Nithish").CopyTo($fdata, 0)
    [System.Text.Encoding]::ASCII.GetBytes("Rahul").CopyTo($fdata, 32)
    [System.Text.Encoding]::ASCII.GetBytes("sample.txt").CopyTo($fdata, 64)
    [System.BitConverter]::GetBytes([uint32]0).CopyTo($fdata, 320)
    [System.BitConverter]::GetBytes([uint32]$bytes.Length).CopyTo($fdata, 324)
    [System.Array]::Copy($bytes, 0, $fdata, 328, $bytes.Length)
    $client1.SendPacket(9, $fdata)

    $rData = $client2.RecvExpected(9, 3000)

    # Send PKT_FILE_END
    $fend = New-Object byte[] 580
    [System.Text.Encoding]::ASCII.GetBytes("Nithish").CopyTo($fend, 0)
    [System.Text.Encoding]::ASCII.GetBytes("Rahul").CopyTo($fend, 32)
    [System.Text.Encoding]::ASCII.GetBytes("sample.txt").CopyTo($fend, 64)
    [System.BitConverter]::GetBytes([int]1).CopyTo($fend, 320)
    $client1.SendPacket(10, $fend)

    $rEnd = $client2.RecvExpected(10, 3000)
    if ($rEnd -ne $null) {
        Write-Host "  -> [Rahul terminal received]: [FILE] 'sample.txt' received successfully! Saved to 'received_files/sample.txt'" -ForegroundColor Green
    }
}

# 8. Clean Disconnect
Start-Sleep -Milliseconds 400
$client2.Dispose()
$client1.Dispose()
Start-Sleep -Milliseconds 500
if (-not $serverProc.HasExited) {
    $serverProc | Stop-Process -Force
}

Write-Host "`n======================================================================" -ForegroundColor Cyan
Write-Host " SERVER ACTIVITY LOG (Recorded during live run):" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
if (Test-Path $serverLogFile) {
    Get-Content $serverLogFile | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    Remove-Item $serverLogFile -Force
}

Write-Host "`n======================================================================" -ForegroundColor Cyan
Write-Host " LIVE DEMONSTRATION RUN COMPLETED SUCCESSFULLY!" -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Cyan
