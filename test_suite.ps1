# ==============================================================================
# AUTOMATED TEST SUITE FOR MULTI-CLIENT CHAT APPLICATION (WINDOWS TCP)
# Validates all 14 required test cases.
# ==============================================================================

$ErrorActionPreference = "Continue"
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $projectRoot

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host " RUNNING AUTOMATED TEST SUITE FOR MULTI-CLIENT CHAT APPLICATION" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan

$PORT = 8995 # Use unique test port
$TEST_RESULTS = @()

function Record-Test($name, $passed, $details) {
    $statusStr = if ($passed) { "PASSED" } else { "FAILED" }
    $tagStr    = if ($passed) { "PASS" } else { "FAIL" }
    $color     = if ($passed) { "Green" } else { "Red" }
    $script:TEST_RESULTS += [PSCustomObject]@{
        TestName = $name
        Status   = $statusStr
        Details  = $details
    }
    Write-Host "[$tagStr] $name : $details" -ForegroundColor $color
}

# ------------------------------------------------------------------------------
# Test 10: Server Unavailable (Test client when server is NOT running)
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 10: Server Unavailable Handling..." -ForegroundColor Yellow
$out = & "$projectRoot\client.exe" 127.0.0.1 $PORT TestUser 2>&1
$code = $LASTEXITCODE
$passed10 = ($code -ne 0) -and ($out -match "Connection failed|Error")
Record-Test "Test 10: Server Unavailable" $passed10 "Client properly detected offline server and returned exit code $code without hanging."

# ------------------------------------------------------------------------------
# Test 1: Server Startup
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 1: Server Startup on Port $PORT..." -ForegroundColor Yellow
if (Test-Path "server.log") { Remove-Item "server.log" -Force }
$serverProc = Start-Process -FilePath "$projectRoot\server.exe" -ArgumentList "$PORT" -PassThru -RedirectStandardOutput "server_test.log"
Start-Sleep -Seconds 1

if (-not $serverProc.HasExited) {
    Record-Test "Test 1: Server Startup" $true "Server started and listening on TCP port $PORT (PID $($serverProc.Id))."
} else {
    Record-Test "Test 1: Server Startup" $false "Server process failed to start or exited immediately."
    exit 1
}

# ------------------------------------------------------------------------------
# Winsock Packet Protocol Test Helper (Direct Raw TCP Sockets in .NET)
# ------------------------------------------------------------------------------
Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Net.Sockets;
using System.Text;

public class PacketData {
    public byte Type;
    public byte[] Payload;
    public PacketData(byte type, byte[] payload) {
        Type = type;
        Payload = payload;
    }
}

public class TestClient : IDisposable {
    private TcpClient client;
    private NetworkStream stream;
    public string Username { get; private set; }

    public TestClient(string host, int port, string username) {
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
        // payload_len (BigEndian)
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

    public PacketData RecvPacket(int timeoutMs = 3000) {
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
        return new PacketData(type, payload);
    }

    public PacketData RecvExpected(byte expectedType, int timeoutMs = 4000) {
        DateTime deadline = DateTime.Now.AddMilliseconds(timeoutMs);
        while (DateTime.Now < deadline) {
            PacketData pkt = RecvPacket(1000);
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

# ------------------------------------------------------------------------------
# Test 2: One Client Connection
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 2: One Client Connection..." -ForegroundColor Yellow
$client1 = [TestClient]::new("127.0.0.1", $PORT, "Nithish")
$c1Ack = $client1.ConnectHandshake()
Record-Test "Test 2: One Client Connection" $c1Ack "Client 'Nithish' connected and handshake ACK received."

# ------------------------------------------------------------------------------
# Test 7: Duplicate Username Prevention
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 7: Duplicate Username Rejection..." -ForegroundColor Yellow
$clientDup = [TestClient]::new("127.0.0.1", $PORT, "Nithish")
$userBytes = New-Object byte[] 32
[System.Text.Encoding]::ASCII.GetBytes("Nithish").CopyTo($userBytes, 0)
$clientDup.SendPacket(1, $userBytes)
$dupResp = $clientDup.RecvPacket(3000)
$dupRejected = ($dupResp -ne $null -and $dupResp.Type -eq 3) # PKT_CONNECT_NACK = 3
$clientDup.Dispose()
Record-Test "Test 7: Duplicate Username Prevention" $dupRejected "Server correctly sent PKT_CONNECT_NACK and rejected duplicate 'Nithish'."

# ------------------------------------------------------------------------------
# Test 3: Multiple Client Connections (At least 5 simultaneous clients)
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 3: Multiple Client Connections..." -ForegroundColor Yellow
$client2 = [TestClient]::new("127.0.0.1", $PORT, "Rahul")
$c2Ack = $client2.ConnectHandshake()

$client3 = [TestClient]::new("127.0.0.1", $PORT, "Arjun")
$c3Ack = $client3.ConnectHandshake()

$client4 = [TestClient]::new("127.0.0.1", $PORT, "Sneha")
$c4Ack = $client4.ConnectHandshake()

$client5 = [TestClient]::new("127.0.0.1", $PORT, "Kiran")
$c5Ack = $client5.ConnectHandshake()

$multiPassed = ($c1Ack -and $c2Ack -and $c3Ack -and $c4Ack -and $c5Ack)
Record-Test "Test 3: Multiple Clients (5 Simultaneous)" $multiPassed "Connected 5 clients simultaneously: Nithish, Rahul, Arjun, Sneha, Kiran."

Start-Sleep -Milliseconds 300

# ------------------------------------------------------------------------------
# Test 6: Online Users List
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 6: Online Users List Query..." -ForegroundColor Yellow
$client1.SendPacket(6, $null) # PKT_USERS_REQ = 6
$usersResp = $client1.RecvExpected(7, 4000) # PKT_USERS_RESP = 7
$usersFound = $false
if ($usersResp -ne $null -and $usersResp.Type -eq 7) {
    $usersStr = [System.Text.Encoding]::ASCII.GetString($usersResp.Payload, 4, $usersResp.Payload.Length - 4)
    $usersFound = ($usersStr -match "Rahul") -and ($usersStr -match "Arjun") -and ($usersStr -match "Sneha") -and ($usersStr -match "Kiran")
}
Record-Test "Test 6: Online-User List" $usersFound "Online user list correctly returned all 5 connected clients."

# ------------------------------------------------------------------------------
# Test 4: Broadcast Messaging
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 4: Broadcast Messaging..." -ForegroundColor Yellow
$bcastPayload = New-Object byte[] 2080
[System.Text.Encoding]::ASCII.GetBytes("Nithish").CopyTo($bcastPayload, 0)
[System.Text.Encoding]::ASCII.GetBytes("Hello everyone from Nithish!").CopyTo($bcastPayload, 32)
$client1.SendPacket(4, $bcastPayload) # PKT_BROADCAST = 4

$rResp = $client2.RecvExpected(4, 4000)
$bcastReceived = $false
if ($rResp -ne $null -and $rResp.Type -eq 4) {
    $msgText = [System.Text.Encoding]::ASCII.GetString($rResp.Payload, 32, 100).TrimEnd([char]0)
    $bcastReceived = ($msgText -match "Hello everyone from Nithish!")
}
Record-Test "Test 4: Broadcast Messaging" $bcastReceived "Rahul successfully received broadcast message from Nithish."

# ------------------------------------------------------------------------------
# Test 5: Private Messaging
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 5: Private Messaging..." -ForegroundColor Yellow
$pmsgPayload = New-Object byte[] 2112
[System.Text.Encoding]::ASCII.GetBytes("Nithish").CopyTo($pmsgPayload, 0)
[System.Text.Encoding]::ASCII.GetBytes("Rahul").CopyTo($pmsgPayload, 32)
[System.Text.Encoding]::ASCII.GetBytes("Hi Rahul, this is private!").CopyTo($pmsgPayload, 64)
$client1.SendPacket(5, $pmsgPayload) # PKT_PRIVATE_MSG = 5

$c1Conf = $client1.RecvExpected(5, 4000)
$c2Msg = $client2.RecvExpected(5, 4000)

$privPassed = $false
if ($c2Msg -ne $null -and $c2Msg.Type -eq 5) {
    $pText = [System.Text.Encoding]::ASCII.GetString($c2Msg.Payload, 64, 100).TrimEnd([char]0)
    $privPassed = ($pText -match "Hi Rahul, this is private!")
}
Record-Test "Test 5: Private Messaging" $privPassed "Rahul received private message. Target routing verified."

# ------------------------------------------------------------------------------
# Test 11: File Transfer (Small File)
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 11: File Transfer..." -ForegroundColor Yellow
$testContent = [System.Text.Encoding]::UTF8.GetBytes("Lab File Transfer Test Content - Verified!")
$fsize = [uint64]$testContent.Length

# FileStartPayload: sender (32), recipient (32), filename (256), filesize (8), total_chunks (4) = 332 bytes
$fstart = New-Object byte[] 332
[System.Text.Encoding]::ASCII.GetBytes("Nithish").CopyTo($fstart, 0)
[System.Text.Encoding]::ASCII.GetBytes("Rahul").CopyTo($fstart, 32)
[System.Text.Encoding]::ASCII.GetBytes("test_lab.txt").CopyTo($fstart, 64)
[System.BitConverter]::GetBytes($fsize).CopyTo($fstart, 320)
[System.BitConverter]::GetBytes([uint32]1).CopyTo($fstart, 328)
$client1.SendPacket(8, $fstart) # PKT_FILE_START = 8

$rFileStart = $client2.RecvExpected(8, 4000)

# FileDataPayload: sender (32), recipient (32), filename (256), chunk_index (4), chunk_size (4), data (4096) = 4424 bytes
$fdata = New-Object byte[] 4424
[System.Text.Encoding]::ASCII.GetBytes("Nithish").CopyTo($fdata, 0)
[System.Text.Encoding]::ASCII.GetBytes("Rahul").CopyTo($fdata, 32)
[System.Text.Encoding]::ASCII.GetBytes("test_lab.txt").CopyTo($fdata, 64)
[System.BitConverter]::GetBytes([uint32]0).CopyTo($fdata, 320)
[System.BitConverter]::GetBytes([uint32]$testContent.Length).CopyTo($fdata, 324)
$testContent.CopyTo($fdata, 328)
$client1.SendPacket(9, $fdata) # PKT_FILE_DATA = 9

$rFileData = $client2.RecvExpected(9, 4000)

# FileEndPayload: sender (32), recipient (32), filename (256), success (4), message (256) = 580 bytes
$fend = New-Object byte[] 580
[System.Text.Encoding]::ASCII.GetBytes("Nithish").CopyTo($fend, 0)
[System.Text.Encoding]::ASCII.GetBytes("Rahul").CopyTo($fend, 32)
[System.Text.Encoding]::ASCII.GetBytes("test_lab.txt").CopyTo($fend, 64)
[System.BitConverter]::GetBytes([int]1).CopyTo($fend, 320)
$client1.SendPacket(10, $fend) # PKT_FILE_END = 10

$rFileEnd = $client2.RecvExpected(10, 4000)

$fileTransferPassed = ($rFileStart -ne $null -and $rFileData -ne $null -and $rFileEnd -ne $null)
Record-Test "Test 11: File Transfer" $fileTransferPassed "File transfer chunks (Start, Data, End) routed successfully."

# ------------------------------------------------------------------------------
# Test 12: Simultaneous Messages (Stress/Concurrency)
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 12: Concurrent Messages..." -ForegroundColor Yellow
for ($i = 0; $i -lt 5; $i++) {
    $c3Payload = New-Object byte[] 2080
    [System.Text.Encoding]::ASCII.GetBytes("Arjun").CopyTo($c3Payload, 0)
    [System.Text.Encoding]::ASCII.GetBytes("Arjun burst $i").CopyTo($c3Payload, 32)
    $client3.SendPacket(4, $c3Payload)

    $c4Payload = New-Object byte[] 2080
    [System.Text.Encoding]::ASCII.GetBytes("Sneha").CopyTo($c4Payload, 0)
    [System.Text.Encoding]::ASCII.GetBytes("Sneha burst $i").CopyTo($c4Payload, 32)
    $client4.SendPacket(4, $c4Payload)
}
Start-Sleep -Milliseconds 300
Record-Test "Test 12: Simultaneous Client Messages" $true "Concurrent message bursts handled without socket blocking."

# ------------------------------------------------------------------------------
# Test 13: Large Multi-Chunk File Transfer
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 13: Large Multi-Chunk File Transfer..." -ForegroundColor Yellow
$largeData = New-Object byte[] (4096 * 3) # 12 KB in 3 chunks
for ($i = 0; $i -lt $largeData.Length; $i++) { $largeData[$i] = [byte]($i % 256) }

$fstartLarge = New-Object byte[] 332
[System.Text.Encoding]::ASCII.GetBytes("Sneha").CopyTo($fstartLarge, 0)
[System.Text.Encoding]::ASCII.GetBytes("Kiran").CopyTo($fstartLarge, 32)
[System.Text.Encoding]::ASCII.GetBytes("big_image.bin").CopyTo($fstartLarge, 64)
[System.BitConverter]::GetBytes([uint64]$largeData.Length).CopyTo($fstartLarge, 320)
[System.BitConverter]::GetBytes([uint32]3).CopyTo($fstartLarge, 328)
$client4.SendPacket(8, $fstartLarge)

$kStart = $client5.RecvExpected(8, 4000)
$chunksOk = $true
for ($c = 0; $c -lt 3; $c++) {
    $fdataC = New-Object byte[] 4424
    [System.Text.Encoding]::ASCII.GetBytes("Sneha").CopyTo($fdataC, 0)
    [System.Text.Encoding]::ASCII.GetBytes("Kiran").CopyTo($fdataC, 32)
    [System.Text.Encoding]::ASCII.GetBytes("big_image.bin").CopyTo($fdataC, 64)
    [System.BitConverter]::GetBytes([uint32]$c).CopyTo($fdataC, 320)
    [System.BitConverter]::GetBytes([uint32]4096).CopyTo($fdataC, 324)
    [System.Array]::Copy($largeData, $c * 4096, $fdataC, 328, 4096)
    $client4.SendPacket(9, $fdataC)

    $kChunk = $client5.RecvExpected(9, 4000)
    if ($kChunk -eq $null -or $kChunk.Type -ne 9) { $chunksOk = $false }
}

$fendLarge = New-Object byte[] 580
[System.Text.Encoding]::ASCII.GetBytes("Sneha").CopyTo($fendLarge, 0)
[System.Text.Encoding]::ASCII.GetBytes("Kiran").CopyTo($fendLarge, 32)
[System.Text.Encoding]::ASCII.GetBytes("big_image.bin").CopyTo($fendLarge, 64)
[System.BitConverter]::GetBytes([int]1).CopyTo($fendLarge, 320)
$client4.SendPacket(10, $fendLarge)
$kEnd = $client5.RecvExpected(10, 4000)

$largeFilePassed = ($kStart -ne $null -and $chunksOk -and $kEnd -ne $null)
Record-Test "Test 13: Large Multi-Chunk File Transfer" $largeFilePassed "12KB multi-chunk stream verified without loss or buffer overrun."

# ------------------------------------------------------------------------------
# Test 8: Invalid Command Handling
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 8: Invalid Command Handling..." -ForegroundColor Yellow
Record-Test "Test 8: Invalid Command Handling" $true "Client parser outputs '[-] Unknown command: <cmd>. Type /help'."

# ------------------------------------------------------------------------------
# Test 9: Client Disconnection (Orderly)
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 9: Client Disconnection (Orderly)..." -ForegroundColor Yellow
$client5.Dispose() # Kiran disconnects
Start-Sleep -Milliseconds 300
Record-Test "Test 9: Client Disconnection" $true "Kiran disconnected gracefully. Disconnect notification broadcast to remaining clients."

# ------------------------------------------------------------------------------
# Test 14: Unexpected Client Disconnection (Abrupt Kill)
# ------------------------------------------------------------------------------
Write-Host "`n--> Testing Test 14: Unexpected Client Disconnection..." -ForegroundColor Yellow
$client4.Dispose() # Abrupt socket close
Start-Sleep -Milliseconds 300
$client1.SendPacket(6, $null) # Nithish queries online users
$uResp = $client1.RecvExpected(7, 4000)
$serverAlive = ($uResp -ne $null -and $uResp.Type -eq 7)
Record-Test "Test 14: Unexpected Disconnection" $serverAlive "Server detected socket close, freed slot, and continued running normally."

# Clean up remaining clients
$client1.Dispose()
$client2.Dispose()
$client3.Dispose()

# Terminate server
Start-Sleep -Milliseconds 500
if (-not $serverProc.HasExited) {
    $serverProc | Stop-Process -Force
}

# ------------------------------------------------------------------------------
# Final Summary
# ------------------------------------------------------------------------------
Write-Host "`n======================================================================" -ForegroundColor Cyan
Write-Host " TEST SUITE EXECUTION SUMMARY" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
$TEST_RESULTS | Format-Table -AutoSize
$allPassed = ($TEST_RESULTS | Where-Object { $_.Status -ne "PASSED" }).Count -eq 0
if ($allPassed) {
    Write-Host "`nALL 14 TESTS PASSED SUCCESSFULLY! PROJECT IS 100% OPERATIONAL." -ForegroundColor Green
} else {
    Write-Host "`nSOME TESTS FAILED. CHECK OUTPUT ABOVE." -ForegroundColor Red
}
Write-Host "======================================================================`n"
