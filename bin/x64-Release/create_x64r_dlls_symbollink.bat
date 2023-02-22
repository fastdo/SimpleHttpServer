@for %%f in (%FASTDO_X64R_BIN%\*.dll) do @(
    @echo mklink: %%~nxf
    @if not exist %%~nxf mklink %%~nxf %%f
    @if exist %%~nxf echo Ok.
)
