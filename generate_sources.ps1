# Verzeichnisse, die durchsucht werden sollen
$directories = @("basics", "bitbase", "eval", "interface", "movegenerator", "pgn", "search")

# Hauptdatei (falls sie nicht in den Verzeichnissen liegt)
$mainFile = "qapla.cpp"

# Alle .cpp-Dateien in den angegebenen Verzeichnissen sammeln
$sourceFiles = $directories | ForEach-Object { Get-ChildItem -Path $_ -Filter "*.cpp" -Recurse } | ForEach-Object { $_.FullName }

# Pfade für das Makefile-Format anpassen
$formattedFiles = $sourceFiles -replace "\\", "/"

# Ausgabe für Makefile
Write-Output "SRCS=$mainFile \"
$formattedFiles | ForEach-Object { Write-Output "    $_ \" }