param([Parameter(Mandatory=$true)][string]$LogDirectory,[string]$OutCsv='events.csv')
$ErrorActionPreference='Stop'
$bad=0;$count=0
$encoding=if($PSVersionTable.PSVersion.Major -ge 6){'utf8BOM'}else{'UTF8'}
Get-ChildItem -LiteralPath $LogDirectory -Filter '*.jsonl' -File | Sort-Object Name | ForEach-Object {
  $source=$_.Name;$number=0
  Get-Content -LiteralPath $_.FullName -Encoding UTF8 | ForEach-Object {
    $number++
    try {
      $row=$_ | ConvertFrom-Json -ErrorAction Stop
      if($row.schema -ne 1){throw 'Unknown schema'}
      $count++
      [pscustomobject]@{file=$source;line=$number;firmware=$row.firmware;time=$row.time;boot=$row.boot;uptime_ms=$row.uptime_ms;
        seq=$row.seq;level=$row.level;event=$row.event;a=$row.a;b=$row.b;detail=$row.detail;
        heap=$row.heap;largest=$row.largest;min_heap=$row.min_heap;psram=$row.psram;rssi=$row.rssi;
        station=$row.station;audio_state=$row.audio_state;buffer=$row.buffer;
        mp3_state=$row.mp3_state;mp3_track=$row.mp3_track;mp3_tracks=$row.mp3_tracks;
        mp3_buffer=$row.mp3_buffer;mp3_progress=$row.mp3_progress}
    } catch {$bad++;Write-Warning ('Invalid/incomplete JSON at {0}:{1}; original file preserved' -f $source,$number)}
  }
} | Export-Csv -LiteralPath $OutCsv -NoTypeInformation -Encoding $encoding
Write-Output ('Exported {0} events; {1} invalid/incomplete lines. Original logs unchanged.' -f $count,$bad)
