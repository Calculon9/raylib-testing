$filePath = "c:\Users\Callum\AppData\Roaming\Code\User\workspaceStorage\fbb37659632c07e55d178496f5de8bf7\GitHub.copilot-chat\transcripts\10ce1de0-fa0b-4d93-a272-372aff451f50.jsonl"
$lines = Get-Content -Path $filePath
$results = [System.Collections.Generic.List[string]]::new()
foreach ($line in $lines) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    try {
        $json = ConvertFrom-Json $line
        # Let's search inside the json deeply
        # Usually it has a "requests" or "interactions" array, or it's a list of turns.
        # Let's search for properties or text inside the JSON object recursively or by converting back/searching.
        $serialized = $line
        if ($serialized -match '## 6|### 6|\b6\.') {
            $results.Add($serialized)
        }
    } catch {
        # ignore or log
    }
}
Write-Output "Found matching lines: $($results.Count)"
if ($results.Count -gt 0) {
    $results[0].Substring(0, [Math]::Min($results[0].Length, 300))
}
