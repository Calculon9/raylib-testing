$filePath = "c:\Users\Callum\AppData\Roaming\Code\User\workspaceStorage\fbb37659632c07e55d178496f5de8bf7\GitHub.copilot-chat\transcripts\10ce1de0-fa0b-4d93-a272-372aff451f50.jsonl"
$lines = Get-Content -Path $filePath
foreach ($line in $lines) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    try {
        $json = ConvertFrom-Json $line
        # Look for messages within interactions/requests
        # Or look for assistant.message type
        if ($json.type -eq "assistant.message") {
            $content = $json.data.content
            if ($content -match 'Centralisation Plan:|Cross-Module Duplication') {
                Write-Output "--- FOUND CENTRALISATION PLAN MESSAGE ---"
                Write-Output $content
                Write-Output "----------------------------------------"
            }
        }
    } catch {
        # ignore
    }
}
