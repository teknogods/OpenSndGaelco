Get-Content .\OpenSndGaelco\src\OpenSndGaelco.rc | ForEach-Object { $_ -replace "1.0.0.0", $APPVEYOR_BUILD_VERSION } | Set-Content .\OpenSndGaelco\src\OpenSndGaelco2.rc
del .\OpenSndGaelco\src\OpenSndGaelco.rc
mv .\OpenSndGaelco\src\OpenSndGaelco2.rc .\OpenSndGaelco\src\OpenSndGaelco.rc